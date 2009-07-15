TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

DBD += pilatusSupport.dbd
DBD += pilatus.dbd

#=============================
# build an ioc application

PROD_IOC = pilatusApp

pilatusApp_SRCS += pilatusROIs.st

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
pilatusApp_SRCS_DEFAULT += pilatus_registerRecordDeviceDriver.cpp pilatusMain.cpp
pilatusApp_SRCS_vxWorks += -nil-

pilatusApp_LIBS += calc
pilatusApp_LIBS += sscan
pilatusApp_LIBS += busy
pilatusApp_LIBS += autosave
pilatusApp_LIBS += stream streamSynApps
pilatusApp_LIBS += asyn
pilatusApp_LIBS += seq pv
pilatusApp_LIBS += $(EPICS_BASE_IOC_LIBS)

pilatusApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary
pilatusApp_SYS_LIBS_cygwin32 += rpc

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

