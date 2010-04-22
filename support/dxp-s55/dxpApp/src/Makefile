TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

# Currently this application only works on vxWorks (CAMAC) and
# Linux (parallel port)

LIBRARY_IOC_Linux   += dxp
LIBRARY_IOC_vxWorks += dxp
LIBRARY_IOC_cygwin32 += dxp
LIBRARY_IOC_WIN32 += dxp

DBD += dxpSupport.dbd
DBD += dxp.dbd
DBD += dxpVX.dbd

USR_CFLAGS += -DXIA_SPECIAL_MEM
USR_CFLAGS += -DEXCLUDE_DGFG200
USR_CFLAGS += -DEXCLUDE_UDXPS

# The following are compiled and added to the Support library
dxp_SRCS += NDDxp.cpp
dxp_SRCS += dxpMED.st

dxp_LIBS += handel
dxp_LIBS += asyn
dxp_LIBS += ADBase NDPlugin netCDF
dxp_LIBS_Linux += PvAPI ImageLib
dxp_LIBS_WIN32 += PvAPI ImageLib
dxp_LIBS += mca 
dxp_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application

# tmm: for support only (no example), disable the App
PROD_IOC_vxWorks += dxpApp
ifeq ($(LINUX_USB_INSTALLED), YES)
PROD_IOC_Linux += dxpApp
endif
PROD_IOC_cygwin32 += dxpApp
PROD_IOC_WIN32 += dxpApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
dxpApp_SRCS_DEFAULT += dxpMain.cpp dxp_registerRecordDeviceDriver.cpp
dxpApp_SRCS_vxWorks += dxpVX_registerRecordDeviceDriver.cpp

# The following adds support from base/src/vxWorks
dxpApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

dxpApp_LIBS += dxp handel
dxpApp_LIBS += mca
dxpApp_LIBS += asyn
dxpApp_LIBS += ADBase NDPlugin netCDF
dxpApp_LIBS_Linux += PvAPI ImageLib
dxpApp_LIBS_WIN32 += PvAPI ImageLib
dxpApp_LIBS += calc sscan autosave
dxpApp_LIBS += busy
dxpApp_LIBS_vxWorks += camac
dxpApp_LIBS += seq pv
dxpApp_LIBS += $(EPICS_BASE_IOC_LIBS)
dxpApp_LIBS_WIN32 += DLPORTIO PlxApi
ifeq ($(LINUX_USB_INSTALLED), YES)
dxpApp_SYS_LIBS_Linux += usb
endif
dxpApp_SYS_LIBS_cygwin32 += DLPORTIO PlxApi
dxpApp_SYS_LIBS_cygwin32 += setupapi
dxpApp_SYS_LIBS_cygwin32 += rpc
dxpApp_SYS_LIBS_WIN32 += setupapi
dxpApp_LIBS_WIN32        += libtiff jpeg libNeXus.dll
dxpApp_LIBS_cygwin32     += libtiff jpeg libNeXus.dll
# To use versions of these libraries supplied with areaDetector use this line
dxpApp_LIBS_Linux        += NeXus mfhdf df hdf5 mxml
# To use system versions of these libraries use this line
#dxpApp_SYS_LIBS_Linux    += NeXus mfhdf df hdf5 mxml

# To use versions of these libraries supplied with areaDetector use this line
dxpApp_LIBS_Linux        += tiff jpeg z
# To use system versions of these libraries use this line
#dxpApp_SYS_LIBS_Linux    += tiff jpeg z

# Test applications
PROD_IOC_Linux += saturn_benchmark1
PROD_IOC_WIN32 += saturn_benchmark1
PROD_IOC_cygwin32 += saturn_benchmark1
saturn_benchmark1_SRCS += saturn_benchmark1.c
saturn_benchmark1_LIBS += dxp
saturn_benchmark1_LIBS += handel asyn
saturn_benchmark1_SYS_LIBS_Linux += usb
saturn_benchmark1_LIBS += $(EPICS_BASE_IOC_LIBS)
saturn_benchmark1_LIBS_WIN32 += DLPORTIO PlxApi
saturn_benchmark1_SYS_LIBS_cygwin32 += DLPORTIO PlxApi
saturn_benchmark1_SYS_LIBS_cygwin32 += setupapi
saturn_benchmark1_SYS_LIBS_cygwin32 += rpc
saturn_benchmark1_SYS_LIBS_WIN32 += setupapi

PROD_IOC_WIN32 += test_scas2
PROD_IOC_cygwin32 += test_scas2
test_scas2_SRCS += test_scas2.c
test_scas2_LIBS += dxp
test_scas2_LIBS += handel asyn
test_scas2_LIBS += $(EPICS_BASE_IOC_LIBS)
test_scas2_LIBS_WIN32 += DLPORTIO PlxApi
test_scas2_SYS_LIBS_cygwin32 += DLPORTIO PlxApi
test_scas2_SYS_LIBS_cygwin32 += setupapi
test_scas2_SYS_LIBS_cygwin32 += rpc
test_scas2_SYS_LIBS_WIN32 += setupapi

#===========================
#===========================
# Build application that lets other apps run with iopl(3)
PROD_IOC_Linux += startWithIopl3
startWithIopl3_SRCS += startWithIopl3.c

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

