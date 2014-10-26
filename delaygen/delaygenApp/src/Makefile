TOP=../..

include $(TOP)/configure/CONFIG
#-----------------------------------------------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================================================================

#=============================================================================
# Build an IOC support library
LIBRARY_IOC += delaygen

delaygen_SRCS += nigpibInterpose.c
delaygen_SRCS += drvAsynDG645.cpp
delaygen_SRCS += drvAsynColby.cpp
delaygen_SRCS += devDG535.c
delaygen_SRCS += drvAsynCoherentSDG.cpp

# This file was moved to custom directory
#delaygen_SRCS += digitalDelay.c

delaygen_LIBS += calc asyn
delaygen_LIBS += $(EPICS_BASE_IOC_LIBS)

delaygen_LIBS_vxWorks += calc asyn TyGSOctal Ipac ip
delaygen_LIBS_vxWorks += $(EPICS_BASE_IOC_LIBS)

#=============================================================================
# Build an IOC application
PROD_IOC = delaygenApp

#-----------------------------------------------------------------------------
# <name>.dbd will be created and installed
DBD += iocdelaygenApp.dbd
DBD += iocdelaygenAppVxWorks.dbd 

DBD += delaygenSupport.dbd

#-----------------------------------------------------------------------------
# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
delaygenApp_SRCS_DEFAULT += iocdelaygenApp_registerRecordDeviceDriver.cpp delaygenMain.cpp
delaygenApp_SRCS_vxWorks += iocdelaygenAppVxWorks_registerRecordDeviceDriver.cpp

#-----------------------------------------------------------------------------
# Include libraries
delaygenApp_LIBS += autosave calc delaygen asyn
ifdef STREAM
	delaygenApp_LIBS += stream
endif
delaygenApp_LIBS_vxWorks += TyGSOctal Ipac ip
delaygenApp_LIBS += $(EPICS_BASE_IOC_LIBS)

delaygenApp_SYS_LIBS_solaris += socket

#
#=============================================================================

include $(TOP)/configure/RULES
#-----------------------------------------------------------------------------
#  ADD RULES AFTER THIS LINE
