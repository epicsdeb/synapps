TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC_vxWorks += devCamac

DBD += devCamacSupport.dbd
DBD += devCamac.dbd

# The following are compiled and added to the Support library
devCamac_SRCS += devCamac.c
devCamac_SRCS += devScalerCamac.c
devCamac_SRCS += devMotorE500.cc
devCamac_SRCS += drvMotorE500.cc

devCamac_LIBS += camac
devCamac_LIBS += $(EPICS_BASE_IOC_LIBS)

# build an ioc application

PROD_IOC_vxWorks = devCamacApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
devCamacApp_SRCS += devCamac_registerRecordDeviceDriver.cpp
devCamacApp_SRCS += devCamacMain.cpp
#The following adds support from base/src/vxWorks
devCamacApp_OBJS += $(EPICS_BASE_BIN)/vxComLibrary

devCamacApp_LIBS += devCamac
devCamacApp_LIBS += camac
devCamacApp_LIBS += std calc sscan
devCamacApp_LIBS += motor softMotor
devCamacApp_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

