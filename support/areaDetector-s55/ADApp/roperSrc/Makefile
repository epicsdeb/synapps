TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

LIBRARY_IOC_WIN32 = roper
#LIBRARY_IOC_cygwin32 = roper
LIB_SRCS += roper.cpp
LIB_LIBS += NDPlugin ADBase netCDF
LIB_LIBS_Linux += PvAPI ImageLib
LIB_LIBS_WIN32 += PvAPI ImageLib
LIB_LIBS += calc busy sscan autosave 
LIB_LIBS += asyn
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC_WIN32 += comTest1
comTest1_SYS_LIBS += ole32 oleaut32
PROD_IOC_WIN32 += comTest2
comTest2_SYS_LIBS += ole32 oleaut32
PROD_IOC_WIN32 += comTest3
comTest3_SYS_LIBS += ole32 oleaut32

PROD_IOC_WIN32 += roperApp
#PROD_IOC_cygwin32 += roperApp
# <name>.dbd will be created from <name>Include.dbd
DBD += roperApp.dbd
DBD += roperSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
roperApp_SRCS_DEFAULT += roperApp_registerRecordDeviceDriver.cpp roperAppMain.cpp

# Add locally compiled object code
roperApp_LIBS += roper
roperApp_LIBS += NDPlugin ADBase netCDF
roperApp_LIBS_Linux += PvAPI ImageLib
roperApp_LIBS_WIN32 += PvAPI ImageLib
roperApp_LIBS += calc busy sscan autosave 
roperApp_LIBS += asyn
roperApp_LIBS += $(EPICS_BASE_IOC_LIBS)

roperApp_LIBS_WIN32    += libtiff jpeg libNeXus.dll
roperApp_LIBS_cygwin32 += libtiff jpeg libNeXus.dll
roperApp_SYS_LIBS      += ole32 oleaut32
# If we don't include nafxcw in the link command then VS links the C runtime before
# MFC and we get errors that operator new and delete are multiply defined.
ifeq ($(T_A), win32-x86)
roperApp_SYS_LIBS      += nafxcw
endif
ifeq ($(T_A), win32-x86-debug)
roperApp_SYS_LIBS      += nafxcwd
endif
roperApp_SYS_LIBS_cygwin32 += rpc

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

