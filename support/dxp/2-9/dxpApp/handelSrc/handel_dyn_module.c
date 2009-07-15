/*
 * handel_dyn_module.c
 *
 *
 * Created 10/03/01 -- PJF
 *
 * This file is nothing more then the routines 
 * that were once in the (now non-existent) file
 * handel_dynamic_config.c. 
 *
 * Copyright (c) 2002,2003,2004, X-ray Instrumentation Associates
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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "xerxes.h"
#include "xerxes_errors.h"
#include "xerxes_structures.h"

#include "xia_handel.h"
#include "handel_generic.h"
#include "handeldef.h"
#include "handel_errors.h"
#include "xia_handel_structures.h"
#include "xia_module.h"
#include "xia_common.h"
#include "xia_assert.h"

static ModName_t KNOWN_MODS[] = {

#ifndef EXCLUDE_DXPX10P
  {"dxpx10p", "dxpx10p"},
  {"saturn",  "dxpx10p"},
  {"x10p",    "dxpx10p"},
#endif /* EXCLUDE_DXPX10P */

#ifndef EXCLUDE_DXP4C2X
  {"dxp4c2x", "dxp4c2x"},
  {"dxp2x4x", "dxp4c2x"},
  {"dxp2x",   "dxp4c2x"},
#endif /* EXCLUDE_DXP4C2X */

#ifndef EXCLUDE_UDXP
  {"udxps",   "udxps"},
#endif /* EXCLUDE_UDXP */

#ifndef EXCLUDE_UDXP
  {"udxp",    "udxp"},
#endif /* EXCLUDE_UDXP */

#ifndef EXCLUDE_XMAP
  {"xmap",    "xmap"},
#endif /* EXCLUDE_XMAP */

#ifndef EXCLUDE_VEGA
  {"vega",   "vega"},
#endif /* EXCLUDE_VEGA */

#ifndef EXCLUDE_MERCURY
  {"mercury", "mercury"},
#endif /* EXCLUDE_MERCURY */
};

#define N_KNOWN_MODS (sizeof(KNOWN_MODS) / sizeof(KNOWN_MODS[0]))

static char *XIA_NULL_STRING = "null";
#define XIA_NULL_STRING_LEN  (strlen(XIA_NULL_STRING) + 1)

HANDEL_STATIC int HANDEL_API xiaProcessInterface(Module *chosen, char *name, void *value);
HANDEL_STATIC int HANDEL_API xiaProcessFirmware(Module *chosen, char *name, void *value);
HANDEL_STATIC int HANDEL_API xiaProcessDefault(Module *chosen, char *name, void *value);
HANDEL_STATIC int _addAlias(Module *chosen, int idx, void *value);
HANDEL_STATIC int _addDetector(Module *chosen, int idx, void *value);
HANDEL_STATIC int _addGain(Module *chosen, int idx, void *value);
HANDEL_STATIC int HANDEL_API xiaGetIFaceInfo(Module *chosen, char *name, void *value);
HANDEL_STATIC int HANDEL_API xiaGetLibrary(Module *chosen, void *value);
HANDEL_STATIC int HANDEL_API xiaGetChannel(Module *chosen, char *name, void *value);
HANDEL_STATIC int HANDEL_API xiaGetFirmwareInfo(Module *chosen, char *name, void *value);
HANDEL_STATIC int HANDEL_API xiaGetDefault(Module *chosen, char *name, void *value);
HANDEL_STATIC boolean_t HANDEL_API xiaIsSubInterface(char *name);
HANDEL_STATIC int HANDEL_API xiaGetAlias(Module *chosen, int chan, void *value);
HANDEL_STATIC int HANDEL_API xiaGetDetector(Module *chosen, int chan, void *value);
HANDEL_STATIC int HANDEL_API xiaGetGain(Module *chosen, int chan, void *value);
HANDEL_STATIC int HANDEL_API xiaGetModuleType(Module *chosen, void *value);
HANDEL_STATIC int HANDEL_API xiaGetNumChans(Module *chosen, void *value);
HANDEL_STATIC int HANDEL_API xiaSetDefaults(Module *module);
HANDEL_STATIC Token HANDEL_API xiaGetNameToken(const char *name);
HANDEL_STATIC int HANDEL_API xiaMergeDefaults(char *output, char *input1, char *input2);

/* This will be the new style for static routines in Handel (10/03) */
HANDEL_STATIC int  _initModule(Module *module, char *alias);
HANDEL_STATIC int  _initDefaults(Module *module);
HANDEL_STATIC int  _initChannels(Module *module);
HANDEL_STATIC int  _initDetectors(Module *module);
HANDEL_STATIC int  _initDetectorChans(Module *module);
HANDEL_STATIC int  _initGains(Module *module);
HANDEL_STATIC int  _initFirmware(Module *module);
HANDEL_STATIC int  _initCurrentFirmware(Module *module);
HANDEL_STATIC int  _initMultiState(Module *module);
HANDEL_STATIC int  _initChanAliases(Module *module);
HANDEL_STATIC int  _addModuleType(Module *module, void *type, char *name);
HANDEL_STATIC int  _addNumChans(Module *module, void *nChans, char *name);
HANDEL_STATIC int  _addChannel(Module *module, void *val, char *name);
HANDEL_STATIC int  _addFirmware(Module *module, void *val, char *name);
HANDEL_STATIC int  _addDefault(Module *module, void *val, char *name);
HANDEL_STATIC int  _addInterface(Module *module, void *val, char *name);
HANDEL_STATIC int  _doAddModuleItem(Module *module, void *data, unsigned int i, char *name);
HANDEL_STATIC int  _splitIdxAndType(char *str, unsigned int *idx, char *type);
HANDEL_STATIC int  _parseDetectorIdx(char *str, int *idx, char *alias);



/* This array should have the string at some index correspond to the Interface
 * constant that that index represents. If a new interface is added a new 
 * string should be added and the row size of interfaceStr should increase
 * by one.
 */
static char *interfaceStr[8] = { 
  "none",
  "j73a",
  "genericSCSI",
  "epp",
  "genericEPP",
  "serial",
  "usb",
  "pxi",
};

/* This array is mainly used to compare names with the possible sub-interface
 * values. This should be update every time a new interface is added.
 */
static char *subInterfaceStr[10] = {
  "scsibus_number",
  "crate_number",
  "slot",
  "epp_address",
  "daisy_chain_id",
  "com_port",
  "baud_rate",
  "device_number",
  "pci_bus",
  "pci_slot",
};


static ModItem_t items[] = {
  {"module_type",        _addModuleType, FALSE_},
  {"number_of_channels", _addNumChans,   TRUE_},
  {"channel",            _addChannel,    TRUE_},
  {"firmware",           _addFirmware,   TRUE_},
  {"default",            _addDefault,    TRUE_},
  {"interface",          _addInterface,  TRUE_},
  {"scsibus_number",     _addInterface,  TRUE_},
  {"crate_number",       _addInterface,  TRUE_},
  {"slot",               _addInterface,  TRUE_},
  {"epp_address",        _addInterface,  TRUE_},
  {"daisy_chain_id",     _addInterface,  TRUE_},
  {"device_number",      _addInterface,  TRUE_},
  {"com_port",           _addInterface,  TRUE_},
  {"baud_rate",          _addInterface,  TRUE_},
  {"pci_bus",            _addInterface,  TRUE_},
  {"pci_slot",           _addInterface,  TRUE_},
};

#define NUM_ITEMS (sizeof(items) / sizeof(items[0]))


static ModInitFunc_t inits[] = {
  _initChannels,
  _initDefaults,
  _initDetectors,
  _initDetectorChans,
  _initGains,
  _initFirmware,
  _initCurrentFirmware,
  _initMultiState,
  _initChanAliases,

};

#define NUM_INITS (sizeof(inits) / sizeof(inits[0]))


static AddChanType_t chanTypes[] = {
  { "alias",    _addAlias },
  { "detector", _addDetector },
  { "gain",     _addGain }

};

#define NUM_CHAN_TYPES (sizeof(chanTypes) / sizeof(chanTypes[0]))


/*****************************************************************************
 * 
 * This routine creates a new Module entry w/ the unique name alias
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaNewModule(char *alias)
{
  int status = XIA_SUCCESS;

  Module *current = NULL;


  /* If HanDeL isn't initialized, go ahead and call it... */
  if (!isHandelInit) 
    {
      status = xiaInitHandel();
      if (status != XIA_SUCCESS) 
        {
          fprintf(stderr, "FATAL ERROR: Unable to load libraries.\n");
          exit(XIA_INITIALIZE);
        }

      xiaLogWarning("xiaNewModule", "HanDeL initialized silently");
    }

  if ((strlen(alias) + 1) > MAXALIAS_LEN) 
    {
      status = XIA_ALIAS_SIZE;
      sprintf(info_string, "Alias contains too many characters");
      xiaLogError("xiaNewModule", info_string, status);
      return status;
    }

  /* Does the module alias already exist? */
  current = xiaFindModule(alias);
  if (current != NULL) 
    {
      status = XIA_ALIAS_EXISTS;
      sprintf(info_string, "Alias %s already in use", alias);
      xiaLogError("xiaNewModule", info_string, status);
      return status;
    }

  /* Initialize linked-list or add to it */
  current = xiaGetModuleHead();
  if (current == NULL) {

    xiaModuleHead = (Module *)handel_md_alloc(sizeof(Module));
    current = xiaModuleHead;
	
  } else {

    while (current->next != NULL) {
		  
	    current = current->next;
    }

    current->next = (Module *)handel_md_alloc(sizeof(Module));
    current = current->next;
  }

  /* Did we actually allocate the memory? */
  if (current == NULL) 
    {
      status = XIA_NOMEM;
      sprintf(info_string, "Unable to allocate memory for Module alias %s", alias);
      xiaLogError("xiaNewModule", info_string, status);
      return status;
    }

	status = _initModule(current, alias);

	if (status != XIA_SUCCESS) {
	  /* XXX: Need to do some cleanup here! */

	  xiaLogError("xiaNewModule", "Error initializing new module", status);
	  return status;
	}

  return XIA_SUCCESS;
}


HANDEL_EXPORT int HANDEL_API xiaAddModuleItem(char *alias, char *name, void *value)
{
  int status;

  unsigned int i;
  unsigned int nItems = (unsigned int)NUM_ITEMS;

  Module *m = NULL;


  /* Verify the arguments */
  if (alias == NULL) {
    status = XIA_NULL_ALIAS;
    xiaLogError("xiaAddModuleItem", "NULL 'alias' passed into function", status);
    return status;
  }

  if (name == NULL) {
    status = XIA_NULL_NAME;
    xiaLogError("xiaAddModuleItem", "NULL 'name' passed into function", status);
    return status;
  }

  if (value == NULL) {
    status = XIA_NULL_VALUE;
    xiaLogError("xiaAddModuleItem", "NULL 'value' passed into function", status);
    return status;
  }

  m = xiaFindModule(alias);

  if (m == NULL) {
    status = XIA_NO_ALIAS;
    sprintf(info_string, "Alias '%s' does not exist in Handel", alias);
    xiaLogError("xiaAddModuleItem", info_string, status);
    return status;
  }
  
  for (i = 0; i < nItems; i++) {
    if (STRNEQ(name, items[i].name)) {
      status = _doAddModuleItem(m, value, i, name);

      if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error adding item '%s' to module '%s'", name,
                m->alias);
        xiaLogError("xiaAddModuleItem", info_string, status);
        return status;
      }

      return XIA_SUCCESS;
    }
  }

  sprintf(info_string, "Unknown item '%s' for module '%s'", name, m->alias);
  xiaLogError("xiaAddModuleItem", info_string, XIA_UNKNOWN_ITEM);
  return XIA_UNKNOWN_ITEM;
}

/*****************************************************************************
 *
 * This routine either returns a pointer to a valid Module entry or NULL (if
 * a valid entry is not found).
 *
 *****************************************************************************/
HANDEL_SHARED Module* HANDEL_API xiaFindModule(char *alias)
{
  unsigned int i;

  char strtemp[MAXALIAS_LEN];
	
  Module *current = NULL;

	
	ASSERT(alias != NULL);


  /* Convert the name to lowercase */
  for (i = 0; i < (unsigned int)strlen(alias); i++) 
    {
      strtemp[i] = (char)tolower(alias[i]);
    }
  strtemp[strlen(alias)] = '\0';

  /* Walk through the linked-list until we find it or we run out of elements */
  current = xiaGetModuleHead();

  while (current != NULL) 
    {
      if (STREQ(strtemp, current->alias)) 
        {
          return current;
        }
		
      current = current->next;
    }

  return NULL;
}


/*****************************************************************************
 * 
 * This routine handles the parsing of the sub-interface elements and also
 * checks to see if an interface has been defined. If an interface doesn't
 * exist, then the routine creates a new interface before adding data to it.
 * If the specified data is for the wrong interface then the routine returns
 * an error.
 *
 * This routine assumes that a valid name is being passed to it. It doesn't
 * error check the name. It just ignores it if invalid.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaProcessInterface(Module *chosen, char *name, void *value)
{
  int status;

	char *interface = NULL;


	ASSERT(chosen != NULL);
	ASSERT(name != NULL);
	ASSERT(value != NULL);



	/* This is a hack to acknowledge the fact that
	 * xiaProcessInterface() now deals with more then
	 * just the sub-interface elements.
	 */
	if (STRNEQ(name, "interface")) {
	  interface = (char *)value;
	
	} else {
	  interface = "";
	}


  /* Decide which interface we are going to be working with */

#ifndef EXCLUDE_CAMAC
  if (STREQ(name, "scsibus_number") ||
      STREQ(name, "crate_number")   ||
      STREQ(name, "slot")           ||
      STREQ(interface, "genericSCSI")    ||
      STREQ(interface, "j73a"))
	  {
      /* Check that this module is really a genericSCSI or j73a */
      if ((chosen->interface_info->type != GENERIC_SCSI)  &&
          (chosen->interface_info->type != JORWAY73A)     &&
          (chosen->interface_info->type != NO_INTERFACE))
        {
          status = XIA_WRONG_INTERFACE;
          sprintf(info_string, "Item %s is not a valid element of the current interface", name);
          xiaLogError("xiaProcessInterface", info_string, status);
          return status;
        }

      /* See if we need to create a new interface */
      if (chosen->interface_info->type == NO_INTERFACE) 
        {
          chosen->interface_info->type = GENERIC_SCSI;
          chosen->interface_info->info.jorway73a = (Interface_Jy73a *)handel_md_alloc(sizeof(Interface_Jy73a));
          if (chosen->interface_info->info.jorway73a == NULL)
            {
              status = XIA_NOMEM;
              xiaLogError("xiaProcessInterface", "Unable to allocate memory for chosen->interface_info->info.jorway73a", status);
              return status;
            }

          chosen->interface_info->info.jorway73a->scsi_bus     = 0;
          chosen->interface_info->info.jorway73a->crate_number = 0;
          chosen->interface_info->info.jorway73a->slot         = 0;

        }

      /* Now go ahead and fill the right field with the right information */
      if (STREQ(name, "scsibus_number")) {
	  
        chosen->interface_info->info.jorway73a->scsi_bus = *((unsigned int *)value);
	  
      } else if (STREQ(name, "crate_number")) {
	  
        chosen->interface_info->info.jorway73a->crate_number = *((unsigned int *)value);
	  
      } else if (STREQ(name, "slot")) {
	  
        chosen->interface_info->info.jorway73a->slot = *((unsigned int *)value);
      }
	
	  } else 
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
		if (STREQ(name, "epp_address")    ||
		    STREQ(name, "daisy_chain_id") ||
        STREQ(interface, "genericEPP")     ||
        STREQ(interface, "epp"))
		  {
        /* Check that the module type is correct */
        if ((chosen->interface_info->type != EPP)           &&
            (chosen->interface_info->type != GENERIC_EPP)   &&
            (chosen->interface_info->type != NO_INTERFACE))
          {
            status = XIA_WRONG_INTERFACE;
            sprintf(info_string, "Item %s is not a valid element of the current interface", name);
            xiaLogError("xiaProcessInterface", info_string, status);
            return status;
          }
	  
        /* See if we need to create a new interface */
        if (chosen->interface_info->type == NO_INTERFACE) 
          {
            chosen->interface_info->type = GENERIC_EPP;
            chosen->interface_info->info.epp = (Interface_Epp *)handel_md_alloc(sizeof(Interface_Epp));
            if (chosen->interface_info->info.epp == NULL)
              {
                status = XIA_NOMEM;
                xiaLogError("xiaProcessInterface", "Unable to allocate memory for chosen->interface_info->info.epp", status);
                return status;
              }
		  
            chosen->interface_info->info.epp->daisy_chain_id = UINT_MAX;
            chosen->interface_info->info.epp->epp_address    = 0x0000;
		  
          }
	  
        if (STREQ(name, "epp_address")) 
          {
            chosen->interface_info->info.epp->epp_address = *((unsigned int *)value);
		  
          } else if (STREQ(name, "daisy_chain_id")) {
		  
          chosen->interface_info->info.epp->daisy_chain_id = *((unsigned int *)value);
        }
	  
      } else 
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
      if ((STREQ(name, "device_number") && chosen->interface_info->type == USB)
          || STREQ(interface, "usb"))
        {
          /* A bit of hack to deal with USB and USB2 both using the
           * "device_number" module item.
           */
          /* Check that the module type is correct */
          if ((chosen->interface_info->type != USB)           &&
              (chosen->interface_info->type != NO_INTERFACE))
            {
              status = XIA_WRONG_INTERFACE;
              sprintf(info_string, "Item %s is not a valid element of the current interface", name);
              xiaLogError("xiaProcessInterface", info_string, status);
              return status;
            }
	  
          /* See if we need to create a new interface */
          if (chosen->interface_info->type == NO_INTERFACE) 
            {
              chosen->interface_info->type = USB;
              chosen->interface_info->info.usb = (Interface_Usb *)handel_md_alloc(sizeof(Interface_Usb));
              if (chosen->interface_info->info.usb == NULL)
                {
                  status = XIA_NOMEM;
                  xiaLogError("xiaProcessInterface", "Unable to allocate memory for chosen->interface_info->info.usb", status);
                  return status;
                }
		  
              chosen->interface_info->info.usb->device_number = 0;
		  
            }
	  
          if (STRNEQ(name, "device_number")) {
            chosen->interface_info->info.usb->device_number = *((unsigned int *)value);
          }

	  
        } else 
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_USB2
        if (STREQ(name, "device_number") || STREQ(interface, "usb2")) {
          /* Check that the module type is correct */
          if (chosen->interface_info->type != USB2 &&
              chosen->interface_info->type != NO_INTERFACE)
            {
              sprintf(info_string, "Item %s is not a valid element of the "
                      "current interface", name);
              xiaLogError("xiaProcessInterface", info_string,
                          XIA_WRONG_INTERFACE);
              return XIA_WRONG_INTERFACE;
            }
	  
          /* See if we need to create a new interface */
          if (chosen->interface_info->type == NO_INTERFACE) {
            chosen->interface_info->type = USB2;
            chosen->interface_info->info.usb2 =
              (Interface_Usb2 *)handel_md_alloc(sizeof(Interface_Usb2));

            if (chosen->interface_info->info.usb2 == NULL) {
              xiaLogError("xiaProcessInterface", "Unable to allocate memory for "
                          "chosen->interface_info->info.usb2", XIA_NOMEM);
              return XIA_NOMEM;
            }
		  
            chosen->interface_info->info.usb2->device_number = 0;
          }
	  
          if (STRNEQ(name, "device_number")) {
            chosen->interface_info->info.usb2->device_number =
              *((unsigned int *)value);
          }
        } else 
#endif /* EXCLUDE_USB2 */

#ifndef EXCLUDE_SERIAL
          if (STREQ(name, "com_port")  ||
              STREQ(name, "baud_rate") ||
              STREQ(interface, "serial"))
            {
	  
              if ((chosen->interface_info->type != SERIAL) &&
                  (chosen->interface_info->type != NO_INTERFACE)) {
		
                status = XIA_WRONG_INTERFACE;
                sprintf(info_string, "Item %s is not a valid element of the current interface", name);
                xiaLogError("xiaProcessInterface", info_string, status);
                return status;
              }
	  
              if (chosen->interface_info->type == NO_INTERFACE) {

                chosen->interface_info->type = SERIAL;
                chosen->interface_info->info.serial = (Interface_Serial *)handel_md_alloc(sizeof(Interface_Serial));

                if (chosen->interface_info->info.serial == NULL) {
			
                  status = XIA_NOMEM;
                  xiaLogError("xiaProcessInterface", "Unable to allocate memory for chosen->interface_info->info.serial",
                              status);
                  return status;
                }
		  
                chosen->interface_info->info.serial->com_port  = 0;
                chosen->interface_info->info.serial->baud_rate = 115200;
              }
 

              if (STREQ(name, "com_port")) {

                chosen->interface_info->info.serial->com_port = *((unsigned int *)value);

              } else if (STREQ(name, "baud_rate")) {

                chosen->interface_info->info.serial->baud_rate = *((unsigned int *)value);
              }

            } else
#endif /* EXCLUDE_SERIAL */

#ifndef EXCLUDE_PLX
            if (STREQ(name, "pci_slot") ||
                STREQ(name, "pci_bus")  ||
                STREQ(interface, "pxi"))
              {
                if ((chosen->interface_info->type != PLX) &&
                    (chosen->interface_info->type != NO_INTERFACE))
                  {
                    sprintf(info_string, "'%s' is not a valid element of the "
                            "currently selected interface", name);
                    xiaLogError("xiaProcessInterface", info_string,
                                XIA_WRONG_INTERFACE);
                    return XIA_WRONG_INTERFACE;
                  }

                if (chosen->interface_info->type == NO_INTERFACE) {
                  chosen->interface_info->type = PLX;
                  chosen->interface_info->info.plx = 
                    (Interface_Plx *)handel_md_alloc(sizeof(Interface_Plx));

                  if (!chosen->interface_info->info.plx) {
                    sprintf(info_string, "Error allocating %d bytes for 'chosen->"
                            "interface_info->info.plx'", sizeof(Interface_Plx));
                    xiaLogError("xiaProcessInterface", info_string, XIA_NOMEM);
                    return XIA_NOMEM;
                  }

                  chosen->interface_info->info.plx->bus  = 0;
                  chosen->interface_info->info.plx->slot = 0;
                }
			
                if (STREQ(name, "pci_slot")) {
                  chosen->interface_info->info.plx->slot = *((byte_t *)value);
			
                } else if (STREQ(name, "pci_bus")) {
                  chosen->interface_info->info.plx->bus  = *((byte_t *)value);
                }
				
              } else
#endif /* EXCLUDE_PLX */
              {
                status = XIA_MISSING_INTERFACE;
                sprintf(info_string, "'%s' is a member of an unknown interface", name);
                xiaLogError("xiaProcessInterace", info_string, status);
                return status;
              }
		  
  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine adds an alias value to the Module information. It is 
 * responsible for allocating memory if the channels[] is NULL. This routine
 * assumes that the data passed to it is valid and consistient with the rest
 * of HanDeL's universe.
 *
 *****************************************************************************/
HANDEL_STATIC int _addAlias(Module *chosen, int idx, void *value)
{
  int status;
  int detChan;

  if (chosen->channels == NULL) 
    {
      /* I should make a point here that the -1 I use in memset is actually cast into
       * an unsigned int/char by memset, so hopefully sign-extension will win...
       */
      chosen->channels = (int *)handel_md_alloc(chosen->number_of_channels * sizeof(int));
      if (chosen->channels == NULL)
        {
          status = XIA_NOMEM;
          xiaLogError("_addAlias", "Unable to allocate memory for chosen->channels", status);
          return status;
        }
		
      memset(chosen->channels, -1, chosen->number_of_channels);
    }

  detChan = *((int *)value);

  if (detChan == -1) 
    {
      /* This isn't really a do anything...I just don't want it to do anything
       * but modify the Mod struct info, which happens later. This is more of 
       * a skip the next part thing...
       */
    } else {
    /* This handles the case where this routine has been called by
     * xiaModifyModuleItem(). This is a little hack, but I don't think 
     * it's too deplorable. Let's just call it 'defensive programming'.
     */
    if (chosen->channels[idx] > -1) 
      {
        status = xiaRemoveDetChan((unsigned int)chosen->channels[idx]);
        chosen->channels[idx] = -1;
        /* The status will be checked a little farther down. (After the add routine is
         * called.
         */
      }

    if (!xiaIsDetChanFree(detChan)) 
      {
        status = XIA_INVALID_DETCHAN;
        sprintf(info_string, "detChan %d is invalid", *((int *)value));
        xiaLogError("_addAlias", info_string, status);
        return status;
      }

    status = xiaAddDetChan(SINGLE, (unsigned int)detChan, (void *)chosen->alias);

    if (status != XIA_SUCCESS) 
      {
        sprintf(info_string, "Error adding detChan %d", detChan);
        xiaLogError("_addAlias", info_string, status);
        return status;
      }
  }

  chosen->channels[idx] = detChan;

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine associates a detector alias to a certain channel. A check is
 * made to verify that the alias actually exists. This routine assumes that
 * the data passed to it is valid and consistient with the rest of HanDeL's
 * universe.
 *
 *****************************************************************************/
HANDEL_STATIC int _addDetector(Module *chosen, int idx, void *value)
{
  int status;
  int i;
	int didx;
	
	char alias[MAXALIAS_LEN];


  Detector *detector = NULL;
	
	
	status = _parseDetectorIdx((char *)value, &didx, alias);
	
	if (status != XIA_SUCCESS) {
	  sprintf(info_string, "Error parsing '%s'", (char *)value);
	  xiaLogError("_addDetector", info_string, status);
	  return status;
	}

	ASSERT(didx >= 0);
   
  detector = xiaFindDetector(alias);

  if (detector == NULL) 
    {
      status = XIA_NO_ALIAS;
      sprintf(info_string, "Detector alias: '%s' does not exist", alias);
      xiaLogError("_addDetector", info_string, status);
      return status;
    }

  if (chosen->detector == NULL) 
    {
      chosen->detector = (char **)handel_md_alloc(chosen->number_of_channels * sizeof(char *));
      if (chosen->detector == NULL)
        {
          status = XIA_NOMEM;
          xiaLogError("_addDetector", "Unable to allocate memory for chosen->detector", status);
          return status;
        }

      for (i = 0; i < (int)chosen->number_of_channels; i++) 
        {
          chosen->detector[i] = (char *)handel_md_alloc(MAXALIAS_LEN * sizeof(char));
          if (chosen->detector[i] == NULL)
            {
              status = XIA_NOMEM;
              xiaLogError("_addDetector", "Unable to allocate memory for chosen->detector[i]", status);
              return status;
            }
        }
    }

  /* This still works even with the syntax change. I'll make my case here
   * in the event that you don't believe me. If value = "detector1:3", the
   * pointer sidx2 in xiaProcessChannel() points at the ':' char, which is then
   * set to '\0'. strcpy only copies up to the first '\0'. BAM!
   */

  strcpy(chosen->detector[idx], alias);

  /* Check that didx is valid... */
  if (didx >= (int)detector->nchan) 
    {
      status = XIA_BAD_CHANNEL;
      sprintf(info_string, "Specified physical detector channel is invalid");
      xiaLogError("_addDetector", info_string, status);
      return status;
    }

  if (chosen->detector_chan == NULL) 
    {
      chosen->detector_chan = (int *)handel_md_alloc(chosen->number_of_channels * sizeof(int));
      if (chosen->detector_chan == NULL)
        {
          status = XIA_NOMEM;
          xiaLogError("_addDetector", "Unable to allocate memory for chosen->detector_chan", status);
          return status;
        }
    }

  chosen->detector_chan[idx] = didx;

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets the channel gain. This routine allocates memory when 
 * appropriate. Like the other routines in this genre, it assumes that
 * the values passed to it are valid and consistient with the rest of the
 * HanDeL universe.
 *
 *****************************************************************************/
HANDEL_STATIC int _addGain(Module *chosen, int idx, void *value)
{

  chosen->gain[idx] = *((double *)value);

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine parses the complete string to determine which actions need
 * to be taken w/r/t the firmware information for the module. The name string
 * passed into this routine is certain to at least contain "firmware" as the
 * first 8 characters and the routine operates on that assumption.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaProcessFirmware(Module *chosen, char *name, void *value)
{
  int status;
  int i;
  int j;
  int idx;

  char *sidx;
  char strtemp[5];

  FirmwareSet *firmware = NULL;


  firmware = xiaFindFirmware((char *)value);
  if (firmware == NULL) {
	  status = XIA_BAD_VALUE;
	  sprintf(info_string, "Firmware alias %s is invalid", (char *)value);
	  xiaLogError("xiaProcessFirmware", info_string, status);
	  return status;
  }

  /* One _possbile_ problem is that memory will be allocated even if the name turns
   * out to be bad. Now, technically, this isn't really that bad since the memory
   * needs to be allocated at some point in time...
   */
  if (chosen->firmware == NULL) 
    {
      chosen->firmware = (char **)handel_md_alloc(chosen->number_of_channels * sizeof(char *));
      if (chosen->firmware == NULL)
        {
          status = XIA_NOMEM;
          xiaLogError("xiaProcessFirmware", "Unable to allocate memory for chosen->firmware", status);
          return status;
        }
	
      for (i = 0; i < (int)chosen->number_of_channels; i++) 
        {
          chosen->firmware[i] = (char *)handel_md_alloc(MAXALIAS_LEN * sizeof(char));
          if (chosen->firmware[i] == NULL)
            {
              status = XIA_NOMEM;
              xiaLogError("xiaProcessFirmware", "Unable to allocate memory for chosen->firmware[i]", status);
              return status;
            }
        }
    }


  /* Determine if the name string is "firmware_set_all" or "firmware_set_chan{n}" */
  sidx = strrchr(name, '_');
	
  if (STREQ(sidx + 1, "all")) 
    {
      for (j = 0; j < (int)chosen->number_of_channels; j++) 
        {
          strcpy(chosen->firmware[j], (char *)value);
        }

      return XIA_SUCCESS;
    }

  /* At this point we have a "chan{n}" name or a bad name. This is done as brute
   * force since I can't think of a clever solution right now. Any takers?
   */
  strncpy(strtemp, sidx + 1, 4);
  strtemp[4] = '\0';
	
  if (STREQ(strtemp, "chan")) 
    {
      idx = atoi(sidx + 5);

      if ((idx >= (int)chosen->number_of_channels) || 
          (idx < 0)) 
        {
          status = XIA_BAD_CHANNEL;
          sprintf(info_string, "Specified channel is invalid");
          xiaLogError("xiaProcessFirmware", info_string, status);
          return status;
        }

      strcpy(chosen->firmware[idx], (char *)value);

    } else {

    status = XIA_BAD_NAME;
    sprintf(info_string, "Invalid name: %s", name);
    xiaLogError("xiaProcessFirmware", info_string, status);
    return status;
  }

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine parses the complete string to determine which actions need
 * to be taken w/r/t the default information for the module. The name string
 * passed into this routine is certain to at least contain "default" as the
 * first 7 characters and the routine operates on that assumption.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaProcessDefault(Module *chosen, char *name, void *value)
{
  int status;
  int j;
  int idx;

  char *sidx;
  char strtemp[5];

  XiaDefaults *defaults = NULL;

  sprintf(info_string, "Preparing to find default %s", (char *)value);
  xiaLogDebug("xiaProcessDefault", info_string);

  defaults = xiaFindDefault((char *)value);
  if (defaults == NULL) 
    {
      status = XIA_NO_ALIAS;
      sprintf(info_string, "Defaults alias %s is invalid", (char *)value);
      xiaLogError("xiaProcessDefault", info_string, status);
      return status;
    }

  sprintf(info_string, "name = %s", name);
  xiaLogDebug("xiaProcessDefault", info_string); 

  /* Determine if the name string is "defaults_all" or "defaults_chan{n}" */
  sidx = strrchr(name, '_');
	
  if (STREQ(sidx + 1, "all")) 
	  {
      for (j = 0; j < (int)chosen->number_of_channels; j++) 
        {
          status = xiaMergeDefaults(chosen->defaults[j], chosen->defaults[j], (char *) value);
			
          if (status != XIA_SUCCESS) 
            {
              sprintf(info_string, "Error merging default %s into default %s", (char *) value, chosen->defaults[j]);
              xiaLogError("xiaProcessDefault", info_string, status);
              return status;
            }
        }
		
      return XIA_SUCCESS;
	  }
	
  /* At this point we have a "chan{n}" name or a bad name. This is done as brute
   * force since I can't think of a clever solution right now. Any takers?
   */
  strncpy(strtemp, sidx + 1, 4);
  strtemp[4] = '\0';
	
  if (STREQ(strtemp, "chan")) 
    {
      idx = atoi(sidx + 5);

      sprintf(info_string, "idx = %d", idx);
      xiaLogDebug("xiaProcessDefault", info_string);

      if ((idx >= (int)chosen->number_of_channels) || 
          (idx < 0)) 
        {
          status = XIA_BAD_CHANNEL;
          sprintf(info_string, "Specified channel is invalid");
          xiaLogError("xiaProcessDefault", info_string, status);
          return status;
        }
	
      sprintf(info_string, "name = %s, new value = %s, old value = %s", name, (char *) value, chosen->defaults[idx]);
      xiaLogDebug("xiaProcessDefault", info_string);
	
      status = xiaMergeDefaults(chosen->defaults[idx], chosen->defaults[idx], (char *) value);
	
      if (status != XIA_SUCCESS) 
        {
          sprintf(info_string, "Error merging default %s into default %s", (char *) value, chosen->defaults[idx]);
          xiaLogError("xiaProcessDefault", info_string, status);
          return status;
        }
	
    } else {
	  
	  status = XIA_BAD_NAME;
	  sprintf(info_string, "Invalid name: %s", name);
	  xiaLogError("xiaProcessDefault", info_string, status);
	  return status;
  }

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine merges 2 default lists into an output list.  The output list
 * can be the same as the input.  List 2 is added to list 1, overriding any
 * common values with the entries in list 2.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaMergeDefaults(char *output, char *input1,
                                              char *input2)
{
  int status; 

  XiaDefaults *inputDefaults1 = NULL;
  XiaDefaults *inputDefaults2 = NULL;

  XiaDaqEntry *current = NULL;

  /* Get all the default pointers */
  inputDefaults1 = xiaFindDefault(input1);
  inputDefaults2 = xiaFindDefault(input2);
  
  /* copy input1 into the output, iff different */
  if (!STREQ(output, input1)) {
    current = inputDefaults1->entry;
    while (current != NULL) {
      status = xiaAddDefaultItem(output, current->name,
                                 (void *) &(current->data));
		  
      if (status != XIA_SUCCESS) {
        sprintf(info_string,
                "Error adding default %s (value = %.3f) to alias %s",
                current->name, current->data, output);
        xiaLogError("xiaMergeDefaults", info_string, status);
        return status;
      }
		  
      current = current->next;
    }
  }
  
  /* Now overwrite with all the values in input2 */
  current = inputDefaults2->entry;
  while (current != NULL) {
    status = xiaAddDefaultItem(output, current->name,
                               (void *) &(current->data));
	  
    if (status != XIA_SUCCESS) {
      sprintf(info_string, "Error adding default %s (value = %.3f) to alias %s",
              current->name, current->data, output);
      xiaLogError("xiaMergeDefaults", info_string, status);
      return status;
    }
	  
    current = current->next;
  }

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine modifies information about a module pointed to by alias.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaModifyModuleItem(char *alias, char *name, void *value)
{
  int status;

  /* Basically a wrapper that filters out module items that we would rather
   * the user not modify once the module has been defined.
   */
  if (STREQ(name, "module_type") ||
      STREQ(name, "number_of_channels"))
    {
      status = XIA_NO_MODIFY;
      sprintf(info_string, "Name: %s can not be modified", name);
      xiaLogError("xiaModifyModuleItem", info_string, status);
      return status;
    }

  status = xiaAddModuleItem(alias, name, value);

  if (status != XIA_SUCCESS) 
    {
      sprintf(info_string, "Error modifying module item: %s", name);
      xiaLogError("xiaModifyModuleItem", info_string, status);
      return status;
    }

  return XIA_SUCCESS;
}
			
/*****************************************************************************
 *
 * This routine retrieves the value specified by name from the Module
 * entry specified by alias.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaGetModuleItem(char *alias, char *name, void *value)
{
  int status;

  Token nameTok = BAD_TOK;

  Module *chosen = NULL;


  chosen = xiaFindModule(alias);
  if (chosen == NULL) 
    {
      status = XIA_NO_ALIAS;
      sprintf(info_string, "Alias %s has not been created", alias);
      xiaLogError("xiaGetModuleItem", info_string, status);
      return status;
    }

  nameTok = xiaGetNameToken(name);

  switch(nameTok)
    {
    default:
    case BAD_TOK:
      status = XIA_BAD_NAME;
      break;

    case MODTYP_TOK:
      status = xiaGetModuleType(chosen, value);
      break;

    case NUMCHAN_TOK:
      status = xiaGetNumChans(chosen, value);
      break;

    case INTERFACE_TOK:
      status = xiaGetIFaceInfo(chosen, name, value);
      break;

    case LIBRARY_TOK:
      status = xiaGetLibrary(chosen, value);
      break;

    case CHANNEL_TOK:
      status = xiaGetChannel(chosen, name, value);
      break;

    case FIRMWARE_TOK:
      status = xiaGetFirmwareInfo(chosen, name, value);
      break;

    case DEFAULT_TOK:
      status = xiaGetDefault(chosen, name, value);
      break;
    }

  if (status != XIA_SUCCESS) 
    {
      sprintf(info_string, "Unable to get value of %s", name);
      xiaLogError("xiaGetModuleItem", info_string, status);
      return status;
    }

  return status;
}


/**********
 * This routine returns the # of modules currently defined
 * in the system. Uses brute force for now until we 
 * develop the appropriate manager for the LL.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetNumModules(unsigned int *numModules)
{
  unsigned int count = 0;

  Module *current = xiaGetModuleHead();


  while (current != NULL) {

    count++;
    current = getListNext(current);
  }

  *numModules = count;
  
  return XIA_SUCCESS;
}


/**********
 * This routine returns the aliases of all of the modules
 * currently defined in the system. Assumes that the 
 * calling routine has allocated the proper amount of
 * memory in "modules".
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetModules(char *modules[])
{
  int i;

  Module *current = xiaGetModuleHead();

  
  for (i = 0; current != NULL; current = getListNext(current)) {

    strcpy(modules[i], current->alias);
  }

  return XIA_SUCCESS;
}
  

/**********
 * This routine is similar to xiaGetModules(), except that
 * it only returns a single alias since VB can't pass a
 * string array into a DLL.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetModules_VB(unsigned int index, char *alias)
{
  int status;

  unsigned int curIdx = 0;

  Module *current = xiaGetModuleHead();


  for (curIdx = 0; current != NULL; current = getListNext(current), curIdx++) {

    if (curIdx == index) {

	    strcpy(alias, current->alias);
	  
	    return XIA_SUCCESS;
    }
  }

  status = XIA_BAD_INDEX;
  sprintf(info_string, "Index = %u is out of range for the modules list", index);
  xiaLogError("xiaGetModules_VB", info_string, status);
  return status;
}


/*****************************************************************************
 *
 * This routine returns a Token corresponding to the appropriate Module item
 * specified by the name. If the name doesn't match any of the known Module
 * items, then BAD_TOK is returned. The token types are enumerated in 
 * xia_module.h.
 *
 *****************************************************************************/
HANDEL_STATIC Token HANDEL_API xiaGetNameToken(const char *name)
{
  int len;

  char *tmpName;
  char *sidx;
	
  Token token = BAD_TOK;

  len = (int)strlen(name);
  tmpName = (char *)handel_md_alloc((len + 1) * sizeof(char));
  strcpy(tmpName, name);

  /* Do the simple tests first and then the tougher ones */
  if (STREQ(tmpName, "module_type")) 
    {
      token = MODTYP_TOK;

    } else if (STREQ(tmpName, "interface") ||
               xiaIsSubInterface(tmpName)) {

    token = INTERFACE_TOK;

  } else if (STREQ(tmpName, "library")) {

    token = LIBRARY_TOK;

  } else if (STREQ(tmpName, "number_of_channels")) {

    token = NUMCHAN_TOK;

  } else {

    sidx = strrchr(tmpName, '_');

    if (sidx != NULL) 
      {
        sidx[0] = '\0';
      }

    if (STREQ(tmpName, "firmware_set")) 
      {
        token = FIRMWARE_TOK;

      } else if (STREQ(tmpName, "default")) {

	    token = DEFAULT_TOK;

    } else if (strncmp(tmpName, "channel", 7) == 0) {

	    token = CHANNEL_TOK;
    }
  }

  handel_md_free((void *)tmpName);
  return token;
}

/*****************************************************************************
 *
 * This routine returns a boolean_t indicating if the passed in name matches
 * any of the known subInterface elements defined in subInterfaceStr[].
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaIsSubInterface(char *name)
{
  int i;

  int len = (int)(sizeof(subInterfaceStr) / sizeof((char *)subInterfaceStr[0]));

  for (i = 0; i < len; i++) 
    {
      if (STREQ(name, subInterfaceStr[i])) 
        {
          return TRUE_;
        }
    }

  return FALSE_;
}

/*****************************************************************************
 *
 * This routine takes a valid interface/sub-interface name and sets value
 * equal to the value as defined in the module pointed to by chosen. It 
 * returns an error if the name corresponds to a sub-interface element that
 * while being valid overall is invalid within the _currently_ defined 
 * interface. 
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetIFaceInfo(Module *chosen, char *name, void *value)
{
  int status;

  if (STREQ(name, "interface")) {
	  strcpy((char *)value, interfaceStr[chosen->interface_info->type]);

  } else 

#ifndef EXCLUDE_CAMAC
	  if (chosen->interface_info->type == GENERIC_SCSI ||
        chosen->interface_info->type == JORWAY73A) {
		
      if (STREQ(name, "scsibus_number")) {
        *((unsigned int *)value) = chosen->interface_info->info.jorway73a->scsi_bus;

      } else if (STREQ(name, "crate_number")) {

        *((unsigned int *)value) = chosen->interface_info->info.jorway73a->crate_number;

      } else if (STREQ(name, "slot")) {

        *((unsigned int *)value) = chosen->interface_info->info.jorway73a->slot;
      }

    } else 
#endif /* EXCLUDE_CAMAC */

#ifndef EXCLUDE_EPP
      if (chosen->interface_info->type == GENERIC_EPP ||
          chosen->interface_info->type == EPP) {
	  
        if (STREQ(name, "epp_address")) {
          *((unsigned int *)value) = chosen->interface_info->info.epp->epp_address;
		  
        } else if (STREQ(name, "daisy_chain_id")) {
		  
          *((unsigned int *)value) = chosen->interface_info->info.epp->daisy_chain_id;
        }
	  
      } else 
#endif /* EXCLUDE_EPP */

#ifndef EXCLUDE_USB
        if (chosen->interface_info->type == USB) {
	  
          if (STREQ(name, "device_number")) {
            *((unsigned int *)value) = chosen->interface_info->info.usb->device_number;
          }
	  
        } else 
#endif /* EXCLUDE_USB */

#ifndef EXCLUDE_SERIAL
          if (chosen->interface_info->type == SERIAL) {
	  
            if (STREQ(name, "com_port")) {
		  
              *((unsigned int *)value) = chosen->interface_info->info.serial->com_port;
		  
            } else if (STREQ(name, "baud_rate")) {
		  
              *((unsigned int *)value) = chosen->interface_info->info.serial->baud_rate;
            }

          } else
#endif /* EXCLUDE_SERIAL */

#ifndef EXCLUDE_PLX
            if (chosen->interface_info->type == PLX) {
		
              if (STREQ(name, "pci_slot")) {
                *((byte_t *)value) = chosen->interface_info->info.plx->slot;

              } else if (STREQ(name, "pci_bus")) {
                *((byte_t *)value) = chosen->interface_info->info.plx->bus;
              }

            } else
#endif /* EXCLUDE_PLX */

              {

                status = XIA_WRONG_INTERFACE;
                sprintf(info_string, "Specified name: %s does not apply to the current interface", name);
                xiaLogError("xiaGetIFaceInfo", info_string, status);
                return status;
              }

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine retrieves the current library name and sets value equal to it.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetLibrary(Module *chosen, void *value)
{
  char *tmp = NULL;

  /* Do nothing until we resolve to put the library in the Module struct
   * or not.
   */
  chosen = chosen;
  tmp = (char *)value;

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine retrieves information about the channel (detector, alias or
 * gain) and sets value equal to it. Assumes that the first 7 characters
 * match "channel" and that 1 underscore is present in name. This routine 
 * does more parsing on the name.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetChannel(Module *chosen, char *name, void *value)
{
  int status;
  int len;
  int chan;

  char *sidx;
  char *tmpName;

  len = (int)strlen(name);
  tmpName = (char *)handel_md_alloc((len + 1) * sizeof(char));
  strcpy(tmpName, name);

  sidx = strchr(tmpName, '_');
  sidx[0] = '\0';

  chan = atoi(tmpName + 7);

  /* Are we getting an alias, detector or gain value? */
  if (STREQ(sidx + 1, "alias")) 
    {
      status = xiaGetAlias(chosen, chan, value);

    } else if (STREQ(sidx + 1, "detector")) {

    status = xiaGetDetector(chosen, chan, value);

  } else if (STREQ(sidx + 1, "gain")) {

    status = xiaGetGain(chosen, chan, value);

  } else {

    status = XIA_BAD_NAME;
  }

  /* We're done with tmpName at this point and we are going to use some
   * conditionals later, so let's free the memory at this point and save 
   * ourselves the effort of having to repeat the free.
   */
  handel_md_free((void *)tmpName);

  if (status != XIA_SUCCESS) 
    {
      sprintf(info_string, "Error getting module information");
      xiaLogError("xiaGetChannel", info_string, status);
      return status;
    }

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets value equal to the alias for channel chan for the 
 * specified module. This routine verifies that chan is within the 
 * proper range.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetAlias(Module *chosen, int chan, void *value)
{
  int status;

  if ((unsigned int)chan >= chosen->number_of_channels) 
    {
      status = XIA_BAD_CHANNEL;
      sprintf(info_string, "Specified channel is out-of-range");
      xiaLogError("xiaGetAlias", info_string, status);
      return status;
    }

	sprintf(info_string, "chosen = %p, chan = %d, alias = %d", chosen, chan,
          chosen->channels[chan]);
	xiaLogDebug("xiaGetAlias", info_string);

  *((int *)value) = chosen->channels[chan];

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets value equal to the detector alias specified for chan. 
 * This routine verfies that chan is within the proper range.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetDetector(Module *chosen, int chan, void *value)
{
  int status;

  /* It's MAXALIAS_LEN + 5 since I want to build a string that has the
   * detector alias name (which can be up to MAXALIAS_LEN chars) concatenated
   * with a colon (+1), up to 3 digits (+3) and termination (+1).
   */
  char strTemp[MAXALIAS_LEN + 5];

  if ((unsigned int)chan >= chosen->number_of_channels) 
    {
      status = XIA_BAD_CHANNEL;
      sprintf(info_string, "Specified channel is out-of-range.");
      xiaLogError("xiaGetDetector", info_string, status);
      return status;
    }

  sprintf(strTemp, "%s:%d%c", chosen->detector[chan], chosen->detector_chan[chan], '\0');

  strcpy((char *)value, strTemp);

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets value equal to the gain value for the specified channel
 * and module. This routine verifies that chan is within the proper range.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetGain(Module *chosen, int chan, void *value)
{
  int status;

  if ((unsigned int)chan >= chosen->number_of_channels) 
    {
      status = XIA_BAD_CHANNEL;
      sprintf(info_string, "Specified channel is out-of-range.");
      xiaLogError("xiaGetGain", info_string, status);
      return status;
    }

  *((double *)value) = chosen->gain[chan];

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine retrieves information about the firmware for the specified
 * module. It assumes that name is, at the very least, "firmware_set". An
 * error is reported if it equals "firmware_set_all" since that is not a 
 * valid choice. 
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetFirmwareInfo(Module *chosen, char *name, void *value)
{
  int status;
  int len;
  int chan;

  char *tmpName;
  char *sidx;

  if (STREQ(name, "firmware_set_all")) 
    {
      status = XIA_BAD_NAME;
      sprintf(info_string, "Must specify channel to retrieve firmware info. from");
      xiaLogError("xiaGetFirmwareInfo", info_string, status);
      return status;
    }

  len = (int)strlen(name);
  tmpName = (char *)handel_md_alloc((len + 1) * sizeof(char));
  strcpy(tmpName, name);
	
  sidx = strrchr(tmpName, '_');
	
  if (strncmp(sidx + 1, "chan", 4) != 0) 
    {
      status = XIA_BAD_NAME;
      sprintf(info_string, "Invalid name");
      xiaLogError("xiaGetFirmwareInfo", info_string, status);

      handel_md_free((void *)tmpName);
		
      return status;
    }

  chan = atoi(sidx + 4);

  if ((unsigned int)chan >= chosen->number_of_channels) 
    {
      status = XIA_BAD_CHANNEL;
      sprintf(info_string, "Specified channel is out-of-range");
      xiaLogError("xiaGetFirmwareInfo", info_string, status);
		
      handel_md_free((void *)tmpName);

      return status;
    }

  strcpy((char *)value, chosen->firmware[chan]);	

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine retrieves information about the default for the specified
 * module. It assumes that name is, at the very least, "default". An
 * error is reported if it equals "default_all" since that is not a 
 * valid choice. 
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetDefault(Module *chosen, char *name, void *value)
{
  int status;
  int len;
  int chan;

  char *tmpName;
  char *sidx;

  if (STREQ(name, "default_all")) 
    {
      status = XIA_BAD_NAME; 
      sprintf(info_string, "Must specify channel to retrieve default info. from");
      xiaLogError("xiaGetDefault", info_string, status);
      return status;
    }

  len = (int)strlen(name);
  tmpName = (char *)handel_md_alloc((len + 1) * sizeof(char));
  strcpy(tmpName, name);
	
  sidx = strrchr(tmpName, '_');
	
  if (strncmp(sidx + 1, "chan", 4) != 0) 
    {
      status = XIA_BAD_NAME;
      sprintf(info_string, "Invalid name");
      xiaLogError("xiaGetDefault", info_string, status);

      handel_md_free((void *)tmpName);
		
      return status;
    }

  sscanf(sidx + 5, "%d", &chan);

  if ((unsigned int)chan >= chosen->number_of_channels) 
    {
      status = XIA_BAD_CHANNEL;
      sprintf(info_string, "Specified channel is out-of-range");
      xiaLogError("xiaGetDefault", info_string, status);
		
      handel_md_free((void *)tmpName);

      return status;
    }

  strcpy((char *)value, chosen->defaults[chan]);	
	
	handel_md_free((void *)tmpName);

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets value equal to the module type for the specified module.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetModuleType(Module *chosen, void *value)
{
  strcpy((char *)value, chosen->type);

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets value equal to the number of channels for the specified
 * module.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetNumChans(Module *chosen, void *value)
{
  *((unsigned int *)value) = chosen->number_of_channels;

  return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine removes a Module entry specifed by alias.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaRemoveModule(char *alias)
{
  int status;
	
  unsigned int size;
  unsigned int i;

  Module *prev    = NULL;
  Module *next    = NULL;
  Module *current = NULL;

  
  current = xiaGetModuleHead();

  if (isListEmpty(current)) 
    {
      status = XIA_NO_ALIAS;
      sprintf(info_string, "Alias %s does not exist", alias);
      xiaLogError("xiaRemoveModule", info_string, status);
      return status;
    }

  next = current->next;

  /* Iterate until we do (or don't) find the Module we are looking for */
  while (!STREQ(alias, current->alias)) 
    {
      prev = current;
      current = next;
      next = current->next;
    }

  if (current == NULL &&
      next    == NULL)
    {
      status = XIA_NO_ALIAS;
      sprintf(info_string, "Alias %s does not exist", alias);
      xiaLogError("xiaRemoveModule", info_string, status);
      return status;
    }

  if (current == xiaGetModuleHead()) 
    {
      xiaModuleHead = current->next;

    } else {

    prev->next = current->next;
  }

  size = current->number_of_channels;
  for (i = 0; i < size; i++) 
    {
      if ((current->channels != NULL) &&
          (current->channels[i] != -1)) 
        {
          xiaRemoveDetChan((unsigned int)current->channels[i]);
        }
    }

  xiaFreeModule(current);

  return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine returns the module channel associated with the specified 
 * detChan. Remember that the module channel value is relative to the module!
 *
 *****************************************************************************/
HANDEL_SHARED unsigned int HANDEL_API xiaGetModChan(unsigned int detChan)
{
  unsigned int i = 0;
	
  char *alias = "\0";

  Module *current = NULL;


  alias = xiaGetAliasFromDetChan((int)detChan);

  current = xiaFindModule(alias);

  for (i = 0; i < current->number_of_channels; i++)
    {
      if (current->channels[i] == (int)detChan)
        {
          return i;
        }
    }

  /* We really shouldn't get here...Need a better way to deal with this.
   * Maybe use signed int so that we can set this to -1 if there is a 
   * problem.
   */
  return 999;
}


/*****************************************************************************
 *
 * Return the head of the Module LL
 *
 *****************************************************************************/
HANDEL_SHARED Module* HANDEL_API xiaGetModuleHead(void)
{
  return xiaModuleHead;
}


/*****************************************************************************
 *
 * This routine sets the NULL defaults for a module to the default set 
 * defined for the specified type.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaSetDefaults(Module *module)
{
  unsigned int m;
  unsigned int n;
  unsigned int k;
  unsigned int numDefaults;

  int j;
  int defLen;
  int status;

  char alias[MAXALIAS_LEN];
  char ignore[MAXALIAS_LEN];

	char *tempAlias = "temporary_defaults";
	
  char **defNames = NULL;
	
  double *defValues = NULL;

  XiaDefaults *defaults = NULL;
  XiaDefaults *tempDefaults = NULL;

  XiaDaqEntry *current = NULL;

  PSLFuncs localFuncs;


  status = xiaLoadPSL(module->type, &localFuncs);
 
  if (status != XIA_SUCCESS) {

    xiaLogError("xiaSetDefaults", "Error loading PSL Functions", status);
    return status;
  }

  numDefaults = localFuncs.getNumDefaults();

  /* First, allocate memory for actual 
   * module defaults.
   */
  module->defaults = (char **)handel_md_alloc(module->number_of_channels * sizeof(char *));

  for (n = 0; n < module->number_of_channels; n++) 
	  {
      module->defaults[n] = (char *)handel_md_alloc(MAXALIAS_LEN * sizeof(char));
		
      if (module->defaults[n] == NULL) 
        {
          status = XIA_NOMEM;
          xiaLogError("xiaSetDefaults", "Unable to allocate memory for module->defaults[i]", status);
          return status;
        }
	  }
	
  if (module->defaults == NULL) 
	  {
      status = XIA_NOMEM;
      xiaLogError("xiaSetDefaults", "Unable to allocate memory for module->defaults", status);
      return status;
	  }

  defNames = (char **)handel_md_alloc(numDefaults * sizeof(char *));

  for (k = 0; k < numDefaults; k++) 
	  {
      defNames[k] = (char *)handel_md_alloc(MAXITEM_LEN * sizeof(char));
	  }
	
  defValues = (double *)handel_md_alloc(numDefaults * sizeof(double));
  
  /* Convert to memset() when I get a chance */
  for (k = 0; k < numDefaults; k++) 
	  {
      defValues[k] = 0.0;
	  }
	
  status = localFuncs.getDefaultAlias(ignore, defNames, defValues);

  if (status != XIA_SUCCESS)
	  {
      for (k = 0; k < numDefaults; k++)
        {
          handel_md_free((void *)defNames[k]);
        }
      handel_md_free((void *)defNames);
      handel_md_free((void *)defValues);
		
      xiaLogError("xiaSetDefaults", "Error getting default alias information", status);
      return status;
	  }

  /* The user no longer controls the defaults dynamically. This
   * is being done by Handel now. We create the default for
   * each channel based on the alias and modChan number.
   */
 
  for (m = 0; m < module->number_of_channels; m++) {

	  sprintf(alias, "defaults_%s_%u", module->alias, m);
	  
	  defaults = xiaFindDefault(alias);
	  
	  /* If defaults is non-NULL then it means that
	   * it was loaded via xiaLoadSystem() and is
	   * valid. Removing a module now removes all
	   * of the defaults associated with that module
	   * so we no longer need to worry about the case 
	   * where the defaults are hanging around from
	   * a prev. module definition.
	   */
	  /* if defaults == NULL, then we need to create
	   * a new list, else we just go and create new 
	   * entries iff there is no entry with a name 
	   * matching one returned from the PSL
	   */
	  if (defaults == NULL) 
      {
        status = xiaNewDefault(alias);
		  
        if (status != XIA_SUCCESS) {
			
          for (k = 0; k < numDefaults; k++) 
            {
              handel_md_free((void *)defNames[k]);
            }
          handel_md_free((void *)defNames);
          handel_md_free((void *)defValues);
			
          sprintf(info_string, "Error creating default with alias %s", alias);
          xiaLogError("xiaSetDefaults", info_string, status);
          return status;
        }
		  
        defaults = xiaFindDefault(alias);
      }
	  
	  /* We must copy the original defaults into a temporary default list, then 
	   * we will fill the original list with the values from the PSL.  Then we 
	   * copy the temporary values into the original (thus overwriting the values
	   * that were originally in the list).  Finally we remove the temporary list
	   */
	  status = xiaNewDefault(tempAlias);
	  
	  if (status != XIA_SUCCESS) {
		
      for (k = 0; k < numDefaults; k++) 
        {
          handel_md_free((void *)defNames[k]);
        }
      handel_md_free((void *)defNames);
      handel_md_free((void *)defValues);
		
      sprintf(info_string, "Error creating %s default list", tempAlias);
      xiaLogError("xiaSetDefaults", info_string, status);
      return status;
	  }
	  
	  tempDefaults = xiaFindDefault(tempAlias);
	  
	  /* copy the original into the temporary */
	  current = defaults->entry;
	  while (current != NULL) 
      {
        status = xiaAddDefaultItem(tempAlias, current->name, (void *) &(current->data));
		  
        if (status != XIA_SUCCESS) 
          {
            for (k = 0; k < numDefaults; k++) 
              {
                handel_md_free((void *)defNames[k]);
              }
            handel_md_free((void *)defNames);
            handel_md_free((void *)defValues);
			  
            sprintf(info_string, "Error adding default %s (value = %.3f) to alias %s", current->name, current->data, tempAlias);
            xiaLogError("xiaSetDefaults", info_string, status);
            return status;
          }
		  
        current = current->next;
      }
	  
	  /* now fill the original with the defaults from the PSL */
	  defLen = (int)numDefaults;
	  
	  for (j = 0; j < defLen; j++) 
      {
        status = xiaAddDefaultItem(alias, defNames[j], (void *)&(defValues[j]));
		  
        if (status != XIA_SUCCESS) 
          {
            for (k = 0; k < numDefaults; k++) 
              {
                handel_md_free((void *)defNames[k]);
              }
            handel_md_free((void *)defNames);
            handel_md_free((void *)defValues);
			  
            sprintf(info_string, "Error adding default %s (value = %.3f) to alias %s", defNames[j], defValues[j], alias);
            xiaLogError("xiaSetDefaults", info_string, status);
            return status;
          }
      }
	
	  /* Finally re-write the original values into the original list */
	  current = tempDefaults->entry;
	  while (current != NULL) 
      {
        status = xiaAddDefaultItem(alias, current->name, (void *) &(current->data));
		  
        if (status != XIA_SUCCESS) 
          {
            for (k = 0; k < numDefaults; k++) 
              {
                handel_md_free((void *)defNames[k]);
              }
            handel_md_free((void *)defNames);
            handel_md_free((void *)defValues);
			  
            sprintf(info_string, "Error adding default %s (value = %.3f) to alias %s", current->name, current->data, alias);
            xiaLogError("xiaSetDefaults", info_string, status);
            return status;
          }
		  
        current = current->next;
      }
	  
	  /* Remove our TEMPORARY defaults list */
	  status = xiaRemoveDefault(tempAlias);
	  
	  if (status != XIA_SUCCESS) 
      {
        for (k = 0; k < numDefaults; k++) 
          {
            handel_md_free((void *)defNames[k]);
          }
        handel_md_free((void *)defNames);
        handel_md_free((void *)defValues);
		  
        sprintf(info_string, "Error removing the %s list", tempAlias);
        xiaLogError("xiaSetDefaults", info_string, status);
        return status;
      }

	  /* set the module entry for this channel to this default list */
	  strcpy(module->defaults[m], alias);
	}	  
	  
	/* clean up our memory */
	for (k = 0; k < numDefaults; k++) 
	  {
      handel_md_free((void *)defNames[k]);
	  }
	handel_md_free((void *)defNames);
	handel_md_free((void *)defValues);

	return XIA_SUCCESS;
}


/**********
 * This determines the absolute channel # of the
 * specified detChan in the specified module. For a 
 * n-channel module, this value is between 0 and n-1.
 **********/
HANDEL_SHARED int HANDEL_API xiaGetAbsoluteChannel(int detChan, Module *module, unsigned int *chan)
{
  unsigned int i;


  for (i = 0; i < module->number_of_channels; i++) {
    if (module->channels[i] == detChan) {
	    *chan = i;
	    return XIA_SUCCESS;
    }
  }

  return XIA_BAD_CHANNEL;
}


/**********
 * Tags all of the "runActive" elements of the specified module
 **********/
HANDEL_SHARED int HANDEL_API xiaTagAllRunActive(Module *module, boolean_t state)
{
  unsigned int i;


  /* An ASSERT should go here indicating
   * that module is indeed a multichannel variant
   */

  /* We also might want to put some sort of 
   * check in here to see if any of the values are
   * already 'state', which could indicate some sort
   * of weird error condition.
   */
  for (i = 0; i < module->number_of_channels; i++) {
    module->state->runActive[i] = state;
  }

  return XIA_SUCCESS;
}


/**********
 * This initializes the passed in module. This routine
 * expects that the memory for the module has already
 * been allocated.
 **********/
HANDEL_STATIC int _initModule(Module *module, char *alias)
{
  int status;

  size_t newAliasLen = strlen(alias) + 1;

  ASSERT(module != NULL);


  /* Set the alias for the new module */
  module->alias = (char *)handel_md_alloc(newAliasLen * sizeof(char));

  if (module->alias == NULL) {
    status = XIA_NOMEM;
    xiaLogError("_initModule",
                "Error allocating memory for module->alias",
                status);
    return status;
  }

  strncpy(module->alias, alias, newAliasLen);

  /* Initialize the hardware interface */
  module->interface_info = (HDLInterface *)handel_md_alloc(sizeof(HDLInterface));

  if (module->interface_info == NULL) {
    status = XIA_NOMEM;
    xiaLogError("_initModule",
                "Error allocating memory for module->interface_info",
                status);
    return status;
  }

  module->interface_info->type = NO_INTERFACE;

  module->type               = NULL;
  module->number_of_channels = 0;
  module->channels           = NULL;
  module->detector           = NULL;
  module->detector_chan      = NULL;
  module->gain               = NULL;
  module->firmware           = NULL;
  module->defaults           = NULL;
  module->currentFirmware    = NULL;
  module->isValidated        = FALSE_;
  module->isMultiChannel     = FALSE_;
  module->state              = NULL;
  module->next               = NULL;
  module->ch                 = NULL;

  return XIA_SUCCESS;
}

HANDEL_STATIC int _addModuleType(Module *module, void *type, char *name)
{
  int status;

  unsigned int i;

  size_t n;

  char *requested = (char *)type;

  UNUSED(name);

  ASSERT(module != NULL);
  ASSERT(type != NULL);


  if (module->type != NULL) {
    status = XIA_TYPE_REDIRECT;
    sprintf(info_string, "Module '%s' already has type '%s'",
            module->alias, module->type);
    xiaLogError("_addModuleType", info_string, status);
    return status;
  }
  
  status = XIA_UNKNOWN_BOARD;
  
  for (i = 0; i < N_KNOWN_MODS; i++) {
    if (STREQ(KNOWN_MODS[i].alias, requested)) {
	  
      n = strlen(KNOWN_MODS[i].actual) + 1;

      module->type = (char *)handel_md_alloc(n);

      if (module->type == NULL) {
        status = XIA_NOMEM;
        xiaLogError("_addModuleType",
                    "Error allocating module->type",
                    status);
        return status;
      }

      strncpy(module->type, KNOWN_MODS[i].actual, n);
      status = XIA_SUCCESS;
      break;
    }
  }

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error finding module type '%s'", requested);
    xiaLogError("_addModuleType", info_string, status);
    return status;
  }

  return XIA_SUCCESS;
}


/**********
 * This particular routine is quite important: once we know
 * the number of channels in the system we have a green light
 * to allocate the rest of the memory in the module
 * structure.
 **********/
HANDEL_STATIC int _addNumChans(Module *module, void *nChans, char *name)
{
  int status;

  unsigned int n        = *((unsigned int *)nChans);
  unsigned int numInits = (unsigned int)NUM_INITS;
  unsigned int i;


  UNUSED(name);

  ASSERT(module != NULL);
  ASSERT(nChans != NULL);


  /* There should be some limits on n, no? Or do we leave
   * that to the PSL verification step? It seems that
   * the sooner we can give feedback, the better off
   * we will be.
   */

  module->number_of_channels = n;

  for (i = 0; i < numInits; i++) {
    status = inits[i](module);

    if (status != XIA_SUCCESS) {
      sprintf(info_string, "Error initializing module '%s' memory (i = %u)",
              module->alias, i);
      xiaLogError("_addNumChans", info_string, status);
      return status;
    }
  }
  
  return XIA_SUCCESS;
}


HANDEL_STATIC int _doAddModuleItem(Module *module, void *data, unsigned int i, char *name)
{
  int status;


  ASSERT(module != NULL);
  ASSERT(data != NULL);
  ASSERT(i < (unsigned int)NUM_ITEMS);


  if (items[i].needsBT) {

    if (module->type == NULL) {
      status = XIA_NEEDS_BOARD_TYPE;
      sprintf(info_string,
              "Item '%s' requires the module ('%s') board_type to be set first",
              items[i].name, module->alias);
      xiaLogError("_doAddModuleItem", info_string, status);
      return status;
    }
  }

  status = items[i].f(module, data, name);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error adding module item '%s' to module '%s'",
            items[i].name, module->alias);
    xiaLogError("_doAddModuleItem", info_string, status);
    return status;
  }

  return XIA_SUCCESS;
}


HANDEL_STATIC int _initDefaults(Module *module)
{
  int status;


  ASSERT(module != NULL);


  /* Right now, we are just calling the xiaSetDefaults()
   * routine, which probably should be reworked and
   * put into this routine.
   */  
  status = xiaSetDefaults(module);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error initializing defaults for module '%s'",
            module->alias);
    xiaLogError("_initDefaults", info_string, status);
    return status;
  }

  return XIA_SUCCESS;
}


/**********
 * Initialize the channel structure
 **********/
HANDEL_STATIC int _initChannels(Module *module)
{
  int status;

  unsigned int i;

  size_t nBytes = module->number_of_channels * sizeof(Channel_t);


  ASSERT(module != NULL);


  module->ch = (Channel_t *)handel_md_alloc(nBytes);

  if (module->ch == NULL) {
    status = XIA_NOMEM;
    sprintf(info_string, "Error allocating %d bytes for module->ch", nBytes);
    xiaLogError("_initChannels", info_string, status);
    return status;
  }

  for (i = 0; i < module->number_of_channels; i++) {
    module->ch[i].n_sca = 0;
    module->ch[i].sca_lo = NULL;
    module->ch[i].sca_hi = NULL;
  }

  return XIA_SUCCESS;
}


HANDEL_STATIC int _initDetectors(Module *module)
{
  unsigned int i;

  size_t nBytes       = module->number_of_channels * sizeof(char *);
  size_t nBytesMaxStr = MAXALIAS_LEN * sizeof(char);


  ASSERT(module != NULL);

  
  module->detector = (char **)handel_md_alloc(nBytes);

  if (module->detector == NULL) {
    sprintf(info_string, "Error allocating %d bytes for module->detector",
            nBytes);
    xiaLogError("_initDetectors", info_string, XIA_NOMEM);
    return XIA_NOMEM;
  }

  for (i = 0; i < module->number_of_channels; i++) {
    module->detector[i] = (char *)handel_md_alloc(nBytesMaxStr);

    if (module->detector[i] == NULL) {
      sprintf(info_string, "Error allocating %d bytes for module->detector[%d]",
              nBytesMaxStr, i);
      xiaLogError("_initDetectors", info_string, XIA_NOMEM);
      return XIA_NOMEM;
    }

    strncpy(module->detector[i], XIA_NULL_STRING, XIA_NULL_STRING_LEN);
  }

  return XIA_SUCCESS;
}


HANDEL_STATIC int _initDetectorChans(Module *module)
{
  unsigned int i;

  size_t nBytes = module->number_of_channels * sizeof(int);


  ASSERT(module != NULL);


  module->detector_chan = (int *)handel_md_alloc(nBytes);

  if (module->detector_chan == NULL) {
    sprintf(info_string, "Error allocating %d bytes for module->detector_chan",
            nBytes);
    xiaLogError("_initDetectorChans", info_string, XIA_NOMEM);
    return XIA_NOMEM;
  }

  for (i = 0; i < module->number_of_channels; i++) {
    module->detector_chan[i] = -1;
  }

  return XIA_SUCCESS;
}


HANDEL_STATIC int _initGains(Module *module)
{
  unsigned int i;

  size_t nBytes = module->number_of_channels * sizeof(double);


  ASSERT(module != NULL);

  
  module->gain = (double *)handel_md_alloc(nBytes);

  if (module->gain == NULL) {
    sprintf(info_string, "Error allocating %d bytes for module->gain",
            nBytes);
    xiaLogError("_initGains", info_string, XIA_NOMEM);
    return XIA_NOMEM;
  }

  for (i = 0; i < module->number_of_channels; i++) {
    module->gain[i] = 1.0;
  }

  return XIA_SUCCESS;
}


HANDEL_STATIC int _initFirmware(Module *module)
{
  unsigned int i;

  size_t nBytes       = module->number_of_channels * sizeof(char *);
  size_t nBytesMaxStr = MAXALIAS_LEN * sizeof(char);


  ASSERT(module != NULL);

  
  module->firmware = (char **)handel_md_alloc(nBytes);

  if (module->firmware == NULL) {
    sprintf(info_string, "Error allocating %d bytes for module->firmware",
            nBytes);
    xiaLogError("_initFirmware", info_string, XIA_NOMEM);
    return XIA_NOMEM;
  }

  for (i = 0; i < module->number_of_channels; i++) {
    module->firmware[i] = (char *)handel_md_alloc(nBytesMaxStr);

    if (module->firmware[i] == NULL) {
      sprintf(info_string, "Error allocating %d bytes for module->detector[%d]",
              nBytesMaxStr, i);
      xiaLogError("_initFirmware", info_string, XIA_NOMEM);
      return XIA_NOMEM;
    }

    strncpy(module->firmware[i], XIA_NULL_STRING, XIA_NULL_STRING_LEN);
  }

  return XIA_SUCCESS;
}


HANDEL_STATIC int _initCurrentFirmware(Module *module)
{
  unsigned int i;

  size_t nBytes = module->number_of_channels * sizeof(CurrentFirmware);

  
  ASSERT(module != NULL);

  
  module->currentFirmware = (CurrentFirmware *)handel_md_alloc(nBytes);

  if (module->currentFirmware == NULL) {
    sprintf(info_string, "Error allocating %d bytes for module->currentFirmware",
            nBytes);
    xiaLogError("_initCurrentFirmware", info_string, XIA_NOMEM);
    return XIA_NOMEM;
  }

  for (i = 0; i < module->number_of_channels; i++) {
    strncpy(module->currentFirmware[i].currentFiPPI,     XIA_NULL_STRING,
            XIA_NULL_STRING_LEN);
    strncpy(module->currentFirmware[i].currentUserFiPPI, XIA_NULL_STRING,
            XIA_NULL_STRING_LEN);
    strncpy(module->currentFirmware[i].currentDSP,       XIA_NULL_STRING,
            XIA_NULL_STRING_LEN);
    strncpy(module->currentFirmware[i].currentUserDSP,   XIA_NULL_STRING,
            XIA_NULL_STRING_LEN);
    strncpy(module->currentFirmware[i].currentMMU,       XIA_NULL_STRING,
            XIA_NULL_STRING_LEN);
    strncpy(module->currentFirmware[i].currentSysFPGA,   XIA_NULL_STRING,
            XIA_NULL_STRING_LEN);
  }

  return XIA_SUCCESS;
}


HANDEL_STATIC int _initMultiState(Module *module)
{
  unsigned int i;

  size_t nBytes     = module->number_of_channels * sizeof(MultiChannelState);
  size_t nBytesFlag = module->number_of_channels * sizeof(boolean_t);
  

  ASSERT(module != NULL);


  if (module->number_of_channels > 1) {
    module->isMultiChannel = TRUE_;

    module->state = (MultiChannelState *)handel_md_alloc(nBytes);

    if (module->state == NULL) {
      sprintf(info_string, "Error allocating %d bytes for module->state",
              nBytes);
      xiaLogError("_initMultiState", info_string, XIA_NOMEM);
      return XIA_NOMEM;
    }

    module->state->runActive = (boolean_t *)handel_md_alloc(nBytesFlag);

    if (module->state->runActive == NULL) {
      sprintf(info_string, "Error allocating %d bytes for module->state->runActive",
              nBytesFlag);
      xiaLogError("_initMultiState", info_string, XIA_NOMEM);
      return XIA_NOMEM;
    }

    for (i = 0; i < module->number_of_channels; i++) {
      module->state->runActive[i] = FALSE_;
    }
  
  } else {
    module->isMultiChannel = FALSE_;
  }

  return XIA_SUCCESS;
}


/**********
 * This routine is responsible for handling any of the items
 * that start with a 'channel' in their name, such as
 * "channel{n}_alias" and "channel{n}_detector".
 **********/
HANDEL_STATIC int _addChannel(Module *module, void *val, char *name)
{
  int status;

  unsigned int i;
  unsigned int nChanTypes = NUM_CHAN_TYPES;
  unsigned int idx = 0;
  
  char type[MAXALIAS_LEN];


  ASSERT(module != NULL);
  ASSERT(val != NULL);
  ASSERT(name != NULL);


  /* 1) Parse off and verify channel number */
  status = _splitIdxAndType(name, &idx, type);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error parsing channel item '%s'", name);
    xiaLogError("_addChannel", info_string, status);
    return status;
  }
  
  if (idx >= module->number_of_channels) {
    sprintf(info_string,
            "Parsed channel '%u' > number channels in module '%u'",
            idx, module->number_of_channels);
    xiaLogError("_addChannel", info_string, XIA_BAD_NAME);
    return XIA_BAD_NAME;
  }

  /* 2) Deal w/ specific: alias, detector, gain */
  for (i = 0; i < nChanTypes; i++) {
    if (STRNEQ(chanTypes[i].name, type)) {
      status = chanTypes[i].f(module, idx, val);

      if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error adding '%s' type to channel %d", 
                type, idx);
        xiaLogError("_addChannel", info_string, status);
        return status;
      }

      return XIA_SUCCESS;
    }
  }
  
  /* _splitIdxAndType does some verification of the
   * type already, so something unusual has happened.
   */
  ASSERT(FALSE_);
  /* Shouldn't return this value because of the ASSERT */
  return XIA_UNKNOWN;
}


HANDEL_STATIC int _splitIdxAndType(char *str, unsigned int *idx, char *type)
{
  size_t i;
  size_t nChanTypes = NUM_CHAN_TYPES;

  char *channel    = NULL;
  char *underscore = NULL;


  ASSERT(str != NULL);
  ASSERT(idx != NULL);
  ASSERT(type != NULL);


  /* NULL out '_' so we have two separate
   * strings to manipulate. String 1
   * will be channel{n} and String 2
   * will be the type of channel item.
   */
  underscore = strrchr(str, '_');
  
  if (underscore == NULL) {
    sprintf(info_string, "Malformed item string: '%s'. Missing '_'", str);
    xiaLogError("_splitIdxAndType", info_string, XIA_BAD_NAME);
    return XIA_BAD_NAME;
  }

  underscore[0] = '\0';
  
  channel = str;

  /* This ASSERT covers an impossible
   * condition since the assumption
   * is that the calling routine has
   * verified that the first part of the
   * string is "channel".
   */
  ASSERT(STRNEQ(channel, "channel"));

  sscanf(channel + CHANNEL_IDX_OFFSET, "%u", idx);

  /* Do some simple verification here?
   * The other option is to require that
   * the calling routine verify the returned
   * values, which I don't think is appropriate
   * in this case.
   */
  underscore++;
  strncpy(type, underscore, strlen(underscore) + 1);

  for (i = 0; i < nChanTypes; i++) {
    if (STRNEQ(type, chanTypes[i].name)) {
      return XIA_SUCCESS;
    }
  }

  sprintf(info_string, "Unknown channel item type: '%s'", type);
  xiaLogError("_splitIdxAndType", info_string, XIA_BAD_NAME);
  return XIA_BAD_NAME;
}


HANDEL_STATIC int _initChanAliases(Module *module)
{
  unsigned int i;

  size_t nBytes = module->number_of_channels * sizeof(int);


  ASSERT(module != NULL);


  module->channels = (int *)handel_md_alloc(nBytes);

  if (module->channels == NULL) {
    sprintf(info_string, "Error allocating %d bytes for module->channels",
            nBytes);
    xiaLogError("_initChanAliases", info_string, XIA_NOMEM);
    return XIA_NOMEM;
  }

  for (i = 0; i < module->number_of_channels; i++) {
    module->channels[i] = -1;
  }

  return XIA_SUCCESS;
}


/**********
 * Parses the index of the detector channel from strings
 * of the form "{detector alias}:{n}". Assumes that
 * the calling function has allocated memory for "alias".
 **********/
HANDEL_STATIC int _parseDetectorIdx(char *str, int *idx, char *alias)
{
  char *colon = NULL;
  

  ASSERT(str != NULL);
  ASSERT(idx != NULL);
  ASSERT(alias != NULL);

  
  colon = strrchr(str, ':');

  if (colon == NULL) {
    sprintf(info_string, "Malformed detector string: '%s'. Missing ':'", str);
    xiaLogError("_parseDetectorIdx", info_string, XIA_BAD_VALUE);
    return XIA_BAD_VALUE;
  }

  colon[0] = '\0';
  colon++;
  
  sscanf(colon, "%d", idx);

  strncpy(alias, str, strlen(str) + 1);
 
  return XIA_SUCCESS;
}


/**********
 * Currently, this routine is a wrapper around xiaProcessFirmware().
 * In the future, xiaProcessFirmware() should be refactored into
 * _addFirmware().
 **********/
HANDEL_STATIC int  _addFirmware(Module *module, void *val, char *name)
{
  int status;


  ASSERT(module != NULL);
  ASSERT(val != NULL);
  ASSERT(name != NULL);


  status = xiaProcessFirmware(module, name, val);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error adding firmware '%s' to module '%s'",
            (char *)val, module->alias);
    xiaLogError("_addFirmware", info_string, status);
    return status;
  }

  return XIA_SUCCESS;
}


/**********
 * Currently, this routine is a wrapper around xiaProcessDefault().
 * In the future, xiaProcessDefault() should be refactored into
 * _addDefault().
 **********/
HANDEL_STATIC int  _addDefault(Module *module, void *val, char *name)
{
  int status;

  
  ASSERT(module != NULL);
  ASSERT(val != NULL);
  ASSERT(name != NULL);

  
  status = xiaProcessDefault(module, name, val);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error adding default '%s' to module '%s'",
            (char *)val, module->alias);
    xiaLogError("_addDefault", info_string, status);
    return status;
  }

  return XIA_SUCCESS;
}


/**********
 * Currently, this routine is a wrapper around xiaProcessInterface().
 * In the future, xiaProcessInterface() should be refactored into
 * _addInterface().
 **********/
HANDEL_STATIC int  _addInterface(Module *module, void *val, char *name)
{
  int status;

  
  ASSERT(module != NULL);
  ASSERT(val != NULL);
  ASSERT(name != NULL);


  status = xiaProcessInterface(module, name, val);

  if (status != XIA_SUCCESS) {
    sprintf(info_string, "Error adding interface component '%s' to module '%s'",
            name, module->alias);
    xiaLogError("_addInterface", info_string, status);
    return status;
  }
  
  return XIA_SUCCESS;
}


/** @brief Returns the module alias for the specified detChan.
 *
 * Assumes that the user has allocated enough memory to hold the entire
 * module alias. The maximum alias size is MAXALIAS_LEN.
 *
 */
HANDEL_EXPORT int HANDEL_API xiaModuleFromDetChan(int detChan, char *alias)
{
  Module *m = xiaGetModuleHead();
  
  unsigned int i;


  if (!alias) {
    xiaLogError("xiaModuleFromDetChan", "'alias' may not be NULL.",
                XIA_NULL_ALIAS);
    return XIA_NULL_ALIAS;
  }

  while (m != NULL) {

    for (i = 0; i < m->number_of_channels; i++) {
      if (detChan == m->channels[i]) {
        strncpy(alias, m->alias, MAXALIAS_LEN);
        return XIA_SUCCESS;
      }
    }

    m = m->next;
  }

  sprintf(info_string, "detChan %d is not defined in any of the known modules",
          detChan);
  xiaLogError("xiaModuleFromDetChan", info_string, XIA_INVALID_DETCHAN);
  return XIA_INVALID_DETCHAN;
}


/** @brief Converts the specified detChan into a detector alias.
 *
 * Assumes that the user has allocated enough memory to hole the entire
 * module alias. The maximum alias size if MAXALIAS_LEN.
 *
 */
HANDEL_EXPORT int HANDEL_API xiaDetectorFromDetChan(int detChan, char *alias)
{
  Module *m = xiaGetModuleHead();

  unsigned int i;

  char *t = NULL;


  if (!alias) {
    xiaLogError("xiaDetectorFromDetChan", "'alias' may not be NULL.",
                XIA_NULL_ALIAS);
    return XIA_NULL_ALIAS;
  }

  while (m != NULL) {
	
    for (i = 0; i < m->number_of_channels; i++) {
      if (detChan == m->channels[i]) {
        /* The detector aliases are stored in the Module structure as
         * "alias:{n}", where "n" represents the physical detector preamplifier
         * that the detChan is bound to. We need to strip that extra information
         * from the alias.
         */
        t = strtok(m->detector[i], ":");
        strcpy(alias, t);
        return XIA_SUCCESS;
      }
    }

    m = m->next;
  }

  sprintf(info_string, "detChan %d is not defined in any of the known modules",
          detChan);
  xiaLogError("xiaDetectorFromDetChan", info_string, XIA_INVALID_DETCHAN);
  return XIA_INVALID_DETCHAN;
}
