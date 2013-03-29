TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc

DBD += cmdButtons.dbd

#  Generate snc main programs
#SNCFLAGS = +m

#  Products
PROD_IOC = cmdButtons

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
cmdButtons_SRCS += cmdButtons_registerRecordDeviceDriver.cpp
cmdButtons_SRCS_DEFAULT += cmdButtonsMain.c
cmdButtons_SRCS_vxWorks += -nil-

#The following adds support from base/src/vxWorks
cmdButtons_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

LIBRARY_IOC += cmdButtonsSupport
cmdButtonsSupport_LIBS += seq pv
cmdButtonsSupport_LIBS += $(EPICS_BASE_IOC_LIBS)
cmdButtonsSupport_SRCS += cmdButtons.st

cmdButtons_LIBS += cmdButtonsSupport seq pv
cmdButtons_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

