TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 
#USR_CXXFLAGS +=

# We only build on Linux and Solaris.  If your architecture has
# libtiff, libjpeg, and libz you can add it here.
LIBRARY_IOC_Linux = pilatusDetector
LIBRARY_IOC_solaris = pilatusDetector
PROD_IOC_Linux += pilatusDetectorApp
PROD_IOC_solaris += pilatusDetectorApp

LIB_SRCS += pilatusDetector.cpp
LIB_SRCS += drvPilatusDetectorEpics.c
LIB_LIBS += NDPlugin ADBase netCDF
LIB_LIBS += calc busy sscan autosave mca
LIB_LIBS += asyn
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)
LIB_SYS_LIBS += tiff jpeg z

#==================================

# <name>.dbd will be created from <name>Include.dbd
DBD += pilatusDetectorApp.dbd
DBD += pilatusDetectorSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
pilatusDetectorApp_SRCS_DEFAULT += pilatusDetectorApp_registerRecordDeviceDriver.cpp pilatusDetectorAppMain.cpp
pilatusDetectorApp_SRCS_vxWorks += pilatusDetectorApp_registerRecordDeviceDriver.cpp

# Add locally compiled object code
pilatusDetectorApp_LIBS += pilatusDetector
pilatusDetectorApp_LIBS += NDPlugin ADBase netCDF
pilatusDetectorApp_LIBS += calc busy sscan autosave mca
pilatusDetectorApp_LIBS += asyn
pilatusDetectorApp_LIBS += $(EPICS_BASE_IOC_LIBS)
pilatusDetectorApp_SYS_LIBS += tiff jpeg z

pilatusDetectorApp_SYS_LIBS_cygwin32 += rpc

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

