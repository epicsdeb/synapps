TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 
USR_CXXFLAGS_Linux += -D_LINUX -D_x86

# The following is needed on win32-x86-debug because the Prosilica library 
# wants to use LIBCMT, and that conflicts with LIBCMTD
ifeq ($(T_A), win32-x86-debug)
USR_LDFLAGS += /NODEFAULTLIB:LIBCMT
endif

LIBRARY_IOC_WIN32 += prosilica
#LIBRARY_IOC_cygwin32 += prosilica
LIBRARY_IOC_Linux += prosilica
LIB_SRCS += prosilica.cpp
LIB_LIBS += NDPlugin ADBase netCDF
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
prosilicaApp_LIBS += NDPlugin ADBase netCDF
prosilicaApp_LIBS_Linux += PvAPI ImageLib
prosilicaApp_LIBS_WIN32 += PvAPI ImageLib
prosilicaApp_LIBS += calc busy sscan autosave 
prosilicaApp_LIBS += asyn
prosilicaApp_LIBS += $(EPICS_BASE_IOC_LIBS)

prosilicaApp_SYS_LIBS_cygwin32 += rpc
prosilicaApp_LIBS_WIN32          +=         jpeg libNeXus.dll
prosilicaApp_LIBS_cygwin32       += libtiff jpeg libNeXus.dll

# To use versions of these libraries supplied with areaDetector use this line
prosilicaApp_LIBS_Linux        += NeXus mfhdf df hdf5 mxml
# To use system versions of these libraries use this line
#prosilicaApp_SYS_LIBS_Linux    += NeXus mfhdf df hdf5 mxml

# To use versions of these libraries supplied with areaDetector use this line
prosilicaApp_LIBS_Linux        += tiff jpeg z
# To use system versions of these libraries use this line
#prosilicaApp_SYS_LIBS_Linux    += tiff jpeg z

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

