TOP=../..

include $(TOP)/configure/CONFIG

-include ../src/CONFIG_STREAM
-include ../../src/CONFIG_STREAM

LIBRARY_DEFAULT = streamSynApps

DBD += $(LIBRARY_DEFAULT).dbd

ifeq ($(LOADABLE_MODULE),YES)
SRCS += $(LIBRARY_DEFAULT)_registerRecordDeviceDriver.cpp
endif 
SRCS += $(SYNAPPS_RECORDS:%=dev%Stream.c)

#LIB_LIBS += stream dbIoc registryIoc iocsh
# In 3.14.10, libiocsh is no longer built
LIB_LIBS += stream dbIoc registryIoc

include $(TOP)/configure/RULES

# create streamSynApps.dbd from all SYNAPPS_RECORDS
$(COMMON_DIR)/$(LIBRARY_DEFAULT).dbd: ../../src/CONFIG_STREAM
	@for r in $(SYNAPPS_RECORDS); \
	do echo "device($$r,INST_IO,dev$${r}Stream,\"stream\")"; \
	done > $@
