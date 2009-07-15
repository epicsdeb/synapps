TOP=../..

include $(TOP)/configure/CONFIG
#-----------------------------------------------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================================================================

#=============================================================================
# Build an IOC support library
LIBRARY_IOC += delaygen

delaygen_SRCS_Linux += nigpibInterpose.c
delaygen_SRCS_Linux += drvAsynDG645.cpp
delaygen_SRCS_Linux += drvAsynColby.cpp
delaygen_SRCS_Linux += digitalDelay.c
delaygen_SRCS_Linux += devDG535.c
delaygen_LIBS_Linux += std asyn
delaygen_LIBS_Linux += $(EPICS_BASE_IOC_LIBS)

delaygen_SRCS_RTEMS += nigpibInterpose.c
delaygen_SRCS_RTEMS += drvAsynDG645.cpp
delaygen_SRCS_RTEMS += drvAsynColby.cpp
delaygen_SRCS_RTEMS += digitalDelay.c
delaygen_SRCS_RTEMS += devDG535.c
delaygen_LIBS_RTEMS += std asyn
delaygen_LIBS_RTEMS += $(EPICS_BASE_IOC_LIBS)

delaygen_SRCS_vxWorks += nigpibInterpose.c
delaygen_SRCS_vxWorks += drvAsynDG645.cpp
delaygen_SRCS_vxWorks += drvAsynColby.cpp
delaygen_SRCS_vxWorks += digitalDelay.c
delaygen_SRCS_vxWorks += devDG535.c
delaygen_LIBS_vxWorks += std asyn TyGSOctal Ipac ip
delaygen_LIBS_vxWorks += $(EPICS_BASE_IOC_LIBS)

#=============================================================================
# Build an IOC application
PROD_IOC_Linux   = delaygenApp
PROD_IOC_RTEMS   = delaygenApp
PROD_IOC_vxWorks = delaygenApp

#-----------------------------------------------------------------------------
# <name>.dbd will be created and installed
DBD += iocdelaygenAppLinux.dbd
DBD += iocdelaygenAppRtemsInclude.dbd
DBD += iocdelaygenAppVxWorksInclude.dbd 

DBD += devDG535.dbd
DBD += drvAsynDG645.dbd
DBD += drvAsynColby.dbd
DBD += digitalDelay.dbd
DBD += nigpibSupport.dbd
DBD += delaygenSupport.dbd

#-----------------------------------------------------------------------------
# <name>.dbd will be created and installed
ifeq ($(OS_CLASS),Linux)
DBD += iocdelaygenAppLinux.dbd
else
ifeq ($(OS_CLASS),RTEMS)
DBD += iocdelaygenAppRtems.dbd
else
ifeq ($(OS_CLASS),vxWorks)
DBD += iocdelaygenAppVxWorks.dbd
endif
endif
endif

#-----------------------------------------------------------------------------
# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
delaygenApp_SRCS_Linux   += iocdelaygenAppLinux_registerRecordDeviceDriver.cpp
delaygenApp_SRCS_Linux   += delaygenMain.cpp
delaygenApp_SRCS_RTEMS   += iocdelaygenAppRtems_registerRecordDeviceDriver.cpp
delaygenApp_SRCS_RTEMS   += delaygenMain.cpp
delaygenApp_SRCS_vxWorks += iocdelaygenAppVxWorks_registerRecordDeviceDriver.cpp
delaygenApp_SRCS_vxWorks += delaygenMain.cpp
delaygenApp_SRCS_vxWorks += -nil-

#-----------------------------------------------------------------------------
# Include libraries
delaygenApp_LIBS_Linux   += autosave std delaygen asyn
delaygenApp_LIBS_Linux   += $(EPICS_BASE_IOC_LIBS)
delaygenApp_LIBS_RTEMS   += autosave std delaygen asyn
delaygenApp_LIBS_RTEMS   += $(EPICS_BASE_IOC_LIBS)
delaygenApp_LIBS_vxWorks += autosave std delaygen asyn TyGSOctal Ipac ip
delaygenApp_LIBS_vxWorks += $(EPICS_BASE_IOC_LIBS)

#
#=============================================================================

include $(TOP)/configure/RULES
#-----------------------------------------------------------------------------
#  ADD RULES AFTER THIS LINE
