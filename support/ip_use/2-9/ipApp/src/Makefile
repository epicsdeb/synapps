TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# build an ioc application

DBD += ip.dbd
DBD += ipVX.dbd

# tmm: couldn't get devMPC to compile for solaris
PROD_IOC_solaris = -nil-
PROD_IOC_DEFAULT = ipApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
ipApp_SRCS_DEFAULT += ip_registerRecordDeviceDriver.cpp ipMain.cpp
ipApp_SRCS_vxWorks += ipVX_registerRecordDeviceDriver.cpp
#The following adds support from base/src/vxWorks
ipApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

ipApp_LIBS += ip calc sscan std
ipApp_LIBS += asyn
ipApp_LIBS += seq pv
ipApp_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

