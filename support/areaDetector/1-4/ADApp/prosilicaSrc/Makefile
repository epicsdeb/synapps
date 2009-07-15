TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 
USR_CXXFLAGS_Linux += -D_LINUX -D_x86

LIBRARY_IOC_WIN32 += prosilica
#LIBRARY_IOC_cygwin32 += prosilica
LIBRARY_IOC_Linux += prosilica
LIB_SRCS += prosilica.cpp
LIB_SRCS += drvProsilicaEpics.c
LIB_LIBS += NDPluginColorConvert NDPlugin ADBase netCDF
LIB_LIBS += PvAPI ImageLib
LIB_LIBS += calc busy sscan autosave
LIB_LIBS += asyn
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC_WIN32 += prosilicaApp
PROD_IOC_Linux += prosilicaApp
#PROD_IOC_cygwin32 += prosilicaApp
# <name>.dbd will be created from <name>Include.dbd
DBD += prosilicaApp.dbd
DBD += prosilicaSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
prosilicaApp_SRCS_DEFAULT += prosilicaApp_registerRecordDeviceDriver.cpp prosilicaAppMain.cpp
prosilicaApp_SRCS_vxWorks += prosilicaApp_registerRecordDeviceDriver.cpp

# Add locally compiled object code
prosilicaApp_LIBS += prosilica
prosilicaApp_LIBS += NDPluginColorConvert NDPlugin ADBase netCDF
prosilicaApp_LIBS += PvAPI ImageLib
prosilicaApp_LIBS += calc busy sscan autosave 
prosilicaApp_LIBS += asyn
prosilicaApp_LIBS += $(EPICS_BASE_IOC_LIBS)

#prosilicaApp_SYS_LIBS_cygwin32 += rpc
prosilicaApp_SYS_LIBS_Linux += tiff jpeg z
#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

