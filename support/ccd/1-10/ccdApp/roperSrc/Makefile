TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 
#USR_CXXFLAGS +=

LIBRARY_IOC += roperCCD
LIB_SRCS += roperCCD.st
#LIB_LIBS += sscan autosave
LIB_LIBS += busy
LIB_LIBS += asyn
LIB_LIBS += seq pv
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

# Note, the following files were manually copied from the roperSrc/bin directory
# to this directory after building with Visual Basic .NET 2003.
BIN_INSTALLS_cygwin32 += ../COMSocketserver.dll ../Interop.JBSOCKETSERVERLib.dll ../Interop.VBA.dll ../Interop.WINX32Lib.dll ../roperServer.exe ../roperServer.pdb
BIN_INSTALLS_WIN32    += ../COMSocketserver.dll ../Interop.JBSOCKETSERVERLib.dll ../Interop.VBA.dll ../Interop.WINX32Lib.dll ../roperServer.exe ../roperServer.pdb
# Note, the following batch file must be run once to register the COMSocketServer.dll with Windows
BIN_INSTALLS_cygwin32 += ../registerCOMSocketserver.bat
BIN_INSTALLS_WIN32    += ../registerCOMSocketserver.bat

#==================================
PROD_IOC += roperCCDApp
# <name>.dbd will be created from <name>Include.dbd
DBD += roperCCDApp.dbd
DBD += roperCCDSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
roperCCDApp_SRCS_DEFAULT += roperCCDApp_registerRecordDeviceDriver.cpp roperCCDMain.cpp
roperCCDApp_SRCS_vxWorks += roperCCDApp_registerRecordDeviceDriver.cpp

# Add locally compiled object code
roperCCDApp_LIBS += roperCCD
#roperCCDApp_LIBS += sscan autosave 
roperCCDApp_LIBS += busy 
roperCCDApp_LIBS += asyn
roperCCDApp_LIBS += seq pv
roperCCDApp_LIBS += $(EPICS_BASE_IOC_LIBS)

roperCCDApp_SYS_LIBS_cygwin32 += rpc

# The following adds support from base/src/vxWorks
roperCCDApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

