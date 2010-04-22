TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_CFLAGS +=   -DWIN32
USR_CXXFLAGS += -DWIN32

LIBRARY_IOC_WIN32    = pvCam
LIBRARY_IOC_cygwin32 = pvCam
LIB_SRCS += pvCam.cpp
LIB_SRCS += drvPVCamEpics.c
LIB_LIBS += NDPlugin ADBase netCDF
LIB_LIBS_Linux += PvAPI ImageLib
LIB_LIBS_WIN32 += PvAPI ImageLib
LIB_LIBS += pvcam32 pv_icl32
LIB_LIBS += calc busy sscan autosave
LIB_LIBS += asyn
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC_WIN32    += pvCamApp
PROD_IOC_cygwin32 += pvCamApp
# <name>.dbd will be created from <name>Include.dbd
DBD += pvCamApp.dbd
DBD += pvCamSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
pvCamApp_SRCS_DEFAULT += pvCamApp_registerRecordDeviceDriver.cpp pvCamAppMain.cpp

# Add locally compiled object code
pvCamApp_LIBS += pvCam
pvCamApp_LIBS += NDPlugin ADBase netCDF
pvCamApp_LIBS_Linux += PvAPI ImageLib
pvCamApp_LIBS_WIN32 += PvAPI ImageLib
pvCamApp_LIBS += pvcam32 pv_icl32
pvCamApp_LIBS += calc busy sscan autosave 
pvCamApp_LIBS += asyn
pvCamApp_LIBS += $(EPICS_BASE_IOC_LIBS)

pvCamApp_SYS_LIBS_cygwin32 += rpc
pvCamApp_LIBS_WIN32        += libtiff jpeg libNeXus.dll
pvCamApp_LIBS_cygwin32     += libtiff jpeg libNeXus.dll

#=============================


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

