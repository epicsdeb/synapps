TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 
#USR_CXXFLAGS +=

LIBRARY_IOC += brukerCCD
#LIB_SRCS += brukerCCD.st
#LIB_SRCS += bruker6000Ctl.stt
LIB_SRCS += smartControl.st
LIB_LIBS += asyn
LIB_LIBS += seq pv
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================

#==================================
PROD_IOC += brukerCCDApp
# <name>.dbd will be created from <name>Include.dbd
DBD += brukerCCDSupport.dbd
DBD += brukerCCDApp.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
PROD_SRCS_DEFAULT += brukerCCDApp_registerRecordDeviceDriver.cpp
PROD_SRCS += brukerCCDMain.cpp

PROD_LIBS += brukerCCD
PROD_LIBS += busy 
#PROD_LIBS += sscan autosave 
PROD_LIBS += asyn
PROD_LIBS += seq pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

# The following adds support from base/src/vxWorks
PROD_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

PROD_SYS_LIBS_cygwin32 += rpc

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

