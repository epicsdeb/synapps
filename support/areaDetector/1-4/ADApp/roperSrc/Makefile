TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 
#USR_CXXFLAGS +=

LIBRARY_IOC_WIN32 = roper
#LIBRARY_IOC_cygwin32 = roper
LIB_SRCS += roper.cpp
LIB_SRCS += drvRoperEpics.c
LIB_LIBS += NDPlugin ADBase netCDF
LIB_LIBS += calc busy sscan autosave 
LIB_LIBS += asyn
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC_WIN32 += comTest1
comTest1_SYS_LIBS += ole32 oleaut32 nafxcw
PROD_IOC_WIN32 += comTest2
comTest2_SYS_LIBS += ole32 oleaut32 nafxcw
PROD_IOC_WIN32 += comTest3
comTest3_SYS_LIBS += ole32 oleaut32 nafxcw

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
roperApp_LIBS += calc busy sscan autosave 
roperApp_LIBS += asyn
roperApp_LIBS += $(EPICS_BASE_IOC_LIBS)

roperApp_SYS_LIBS += ole32 oleaut32 nafxcw
roperApp_SYS_LIBS_cygwin32 += rpc

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

