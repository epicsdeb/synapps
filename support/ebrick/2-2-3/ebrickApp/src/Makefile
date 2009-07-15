TOP=../..

include $(TOP)/configure/CONFIG
#-----------------------------------------------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================================================================

#=============================================================================
# Build an IOC support library
LIBRARY_IOC += ebrick

#-----------------------------------------------------------------------------
# The following are compiled and added to the Support library
ebrick_SRCS_Linux += drvSen.cpp
ebrick_SRCS_Linux += drvDio.cpp
ebrick_SRCS_Linux += drvAdc.cpp
ebrick_SRCS_Linux += drvDac.cpp
ebrick_SRCS_Linux += devAsynScaler059.cpp
ebrick_SRCS_Linux += devAsynScaler063.cpp
ebrick_SRCS_Linux += drvBoardInit.cpp
ebrick_SRCS_Linux += drvOmsPcx8.cpp
ebrick_SRCS_Linux += devSysMon.c
ebrick_SRCS_Linux += drvAsynSerialLink.cpp
ebrick_LIBS_Linux += calc sscan asyn
ebrick_LIBS_Linux += $(EPICS_BASE_IOC_LIBS)

ebrick_SRCS_RTEMS += drvAsynC5282.cpp
ebrick_SRCS_RTEMS += devAsynScaler071.cpp
ebrick_SRCS_RTEMS += drvAsynSerialLink.cpp
ebrick_LIBS_RTEMS += calc sscan asyn
ebrick_LIBS_RTEMS += $(EPICS_BASE_IOC_LIBS)

ebrick_SRCS_vxWorks += drvAsynSerialLink.cpp
ebrick_LIBS_vxWorks += calc sscan asyn
ebrick_LIBS_vxWorks += $(EPICS_BASE_IOC_LIBS)
#
#==============================================================================

#-----------------------------------------------------------------------------
# Install database definition files into ebrick/dbd
DBD += iocebrickAppLinuxInclude.dbd 
DBD += iocebrickAppRtemsInclude.dbd
DBD += iocebrickAppVxWorksInclude.dbd 

#=============================================================================
# Build an IOC application
PROD_IOC_Linux = ebrickApp
PROD_IOC_RTEMS = ebrickApp

#-----------------------------------------------------------------------------
# <name>.dbd will be created and installed
ifeq ($(OS_CLASS),Linux)
DBD += iocebrickAppLinux.dbd
else
ifeq ($(OS_CLASS),RTEMS)
DBD += iocebrickAppRtems.dbd
else
ifeq ($(OS_CLASS),vxWorks)
DBD += iocebrickAppVxWorks.dbd
endif
endif
endif

#-----------------------------------------------------------------------------
# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
ebrickApp_SRCS_Linux += iocebrickAppLinux_registerRecordDeviceDriver.cpp
ebrickApp_SRCS_Linux += ebrickMain.cpp
ebrickApp_SRCS_RTEMS += iocebrickAppRtems_registerRecordDeviceDriver.cpp
ebrickApp_SRCS_RTEMS += ebrickMain.cpp

#-----------------------------------------------------------------------------
# Include libraries
ebrickApp_LIBS_Linux += calc sscan autosave ebrick std asyn seq pv
ebrickApp_LIBS_Linux += $(EPICS_BASE_IOC_LIBS)
ebrickApp_LIBS_RTEMS += calc sscan autosave ebrick std asyn seq pv epicsMCF5282
ebrickApp_LIBS_RTEMS += $(EPICS_BASE_IOC_LIBS)

#
#=============================================================================


#=============================================================================
# Build an application
PROD_HOST_Linux = launch launchr
launch_SRCS_Linux += launch.c
launchr_SRCS_Linux += launchr.c
#
#=============================================================================


include $(TOP)/configure/RULES
#-----------------------------------------------------------------------------
#  ADD RULES AFTER THIS LINE
