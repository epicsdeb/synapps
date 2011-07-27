TOP=../..

include $(TOP)/configure/CONFIG
#-----------------------------------------------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================================================================


#=============================================================================
# Build an IOC support library
LIBRARY_IOC += vac

#-----------------------------------------------------------------------------
# Install database definition files into <top>/dbd
#DBD += devXXX.dbd


DBDINC += vsRecord
DBDINC += digitelRecord
vac_SRCS += vsRecord.c
vac_SRCS += digitelRecord.c

#-----------------------------------------------------------------------------
# The following are compiled and added to the Support library
vac_SRCS += devDigitelPump.c
vac_SRCS += devVacSen.c

vac_LIBS += asyn
vac_LIBS += $(EPICS_BASE_IOC_LIBS)
#
#==============================================================================


#=============================================================================
# Build an ioc application <name>
# Don't need for /APSshare --tmm 10/16/2007
#PROD_IOC = vacApp

#-----------------------------------------------------------------------------
# <name>.dbd and <name>VX.dbd will be created and installed
DBD += vacApp.dbd
DBD += vacAppVX.dbd
DBD += vacSupport.dbd
DBD += devDigitelPump.dbd
DBD += devVacSen.dbd

#-----------------------------------------------------------------------------
# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
# <name>VX_registerRecordDeviceDriver.cpp will be created from <name>VX.dbd
vacApp_SRCS_DEFAULT += vacApp_registerRecordDeviceDriver.cpp
vacApp_SRCS_vxWorks += vacAppVX_registerRecordDeviceDriver.cpp

#-----------------------------------------------------------------------------
# Include application sources
vacApp_SRCS_DEFAULT += vacAppMain.cpp
# We'll use the original copy in IP
#vacApp_SRCS_vxWorks += tyGSAsynInit.c
vacApp_SRCS_vxWorks += -nil-

#-----------------------------------------------------------------------------
# Include support from base/src/vxWorks
vacApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

#-----------------------------------------------------------------------------
# Include libraries
vacApp_Common_LIBS  += vac asyn
vacApp_Common_LIBS  += $(EPICS_BASE_IOC_LIBS)
vacApp_vxWorks_LIBS += TyGSOctal Ipac

vacApp_LIBS_DEFAULT += $(vacApp_Common_LIBS)
vacApp_LIBS_vxWorks += $(vacApp_Common_LIBS) $(vacApp_vxWorks_LIBS)
#
#==============================================================================

include $(TOP)/configure/RULES
#------------------------------------------------------------------------------
#  ADD RULES AFTER THIS LINE

