TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 
#USR_CXXFLAGS +=

LIBRARY_IOC += marCCD
LIB_SRCS += marCCD.st
LIB_LIBS += asyn
LIB_LIBS += seq pv
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)


#==================================
PROD_IOC_Linux += marccd_server_socket
marccd_server_socket_SRCS +=  marccd_server_socket.c dsmar_utils.c socket_utils.c

#==================================
PROD_IOC += marCCDApp
# <name>.dbd will be created from <name>Include.dbd
DBD += marCCDApp.dbd
DBD += marCCDSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
marCCDApp_SRCS_DEFAULT += marCCDApp_registerRecordDeviceDriver.cpp marCCDMain.cpp
marCCDApp_SRCS_vxWorks += -nil-
marCCDApp_LIBS += marCCD
marCCDApp_LIBS += asyn
marCCDApp_LIBS += busy 
marCCDApp_LIBS += autosave 
marCCDApp_LIBS += seq pv
marCCDApp_LIBS += $(EPICS_BASE_IOC_LIBS)

marCCDApp_SYS_LIBS_cygwin32 += rpc

# The following adds support from base/src/vxWorks
PROD_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary


#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

