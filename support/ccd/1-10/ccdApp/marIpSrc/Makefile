TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

DIRS += ioSrc
DIRS += CBFSrc 
#include $(TOP)/configure/RULES_DIRS

#USR_CFLAGS += 
#USR_CXXFLAGS +=

LIBRARY_IOC_Linux += marIp
LIB_SRCS += scan345.c command.c marclock.c marconfig.c mardisk.c marhw.c
LIB_SRCS += marnet.c marstartup.c marxf.c misc.c pck.c
LIB_LIBS += marIpIo marIpCBF
LIB_LIBS += asyn
LIB_LIBS += seq pv
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC_Linux += marIpApp
# <name>.dbd will be created from <name>Include.dbd
DBD += marIpApp.dbd
DBD += marIpSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
marIpApp_SRCS += marIpApp_registerRecordDeviceDriver.cpp
marIpApp_SRCS_DEFAULT += marIpMain.cpp
marIpApp_SRCS_vxWorks += -nil-
marIpApp_LIBS += marIp
marIpApp_LIBS += asyn
#marIpApp_LIBS += sscan autosave 
marIpApp_LIBS += busy 
marIpApp_LIBS += seq pv
marIpApp_LIBS += $(EPICS_BASE_IOC_LIBS)

# The following adds support from base/src/vxWorks
PROD_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary


#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

