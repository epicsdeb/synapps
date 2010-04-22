TOP=../..

include $(TOP)/configure/CONFIG
#-----------------------------------------------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================================================================


#=============================================================================
# Build an IOC support library
LIBRARY_IOC += love

#-----------------------------------------------------------------------------
# Install database definition files into <top>/dbd
DBD += devLove.dbd

#-----------------------------------------------------------------------------
# The following are compiled and added to the Support library
love_SRCS += drvLove.c

love_LIBS += asyn
love_LIBS += $(EPICS_BASE_IOC_LIBS)
#
#==============================================================================


#=============================================================================
# Build an ioc application <name>
PROD_IOC = loveApp

#-----------------------------------------------------------------------------
# <name>.dbd and <name>VX.dbd will be created and installed
DBD += loveApp.dbd
DBD += loveAppVX.dbd

#-----------------------------------------------------------------------------
# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
# <name>VX_registerRecordDeviceDriver.cpp will be created from <name>VX.dbd
loveApp_SRCS_DEFAULT += loveApp_registerRecordDeviceDriver.cpp loveMain.cpp
loveApp_SRCS_vxWorks += loveAppVX_registerRecordDeviceDriver.cpp

#-----------------------------------------------------------------------------
# Include support from base/src/vxWorks
loveApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

#-----------------------------------------------------------------------------
# Include libraries
loveApp_Common_LIBS  += love asyn
loveApp_Common_LIBS  += $(EPICS_BASE_IOC_LIBS)
loveApp_vxWorks_LIBS += TyGSOctal Ipac

loveApp_LIBS_DEFAULT += $(loveApp_Common_LIBS)
loveApp_LIBS_vxWorks += $(loveApp_Common_LIBS) $(loveApp_vxWorks_LIBS)
#
#==============================================================================

include $(TOP)/configure/RULES
#------------------------------------------------------------------------------
#  ADD RULES AFTER THIS LINE

