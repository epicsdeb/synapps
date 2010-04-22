TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 
#USR_CXXFLAGS +=

# We only build on Linux and Solaris.  If your architecture has
# libtiff, libjpeg, and libz you can add it here.
# We add _AD (for areaDetector) to the library name to avoid conflict
# with the library marCCD in the ccd module
LIBRARY_IOC_Linux = marCCD_AD
LIBRARY_IOC_solaris = marCCD_AD
PROD_IOC_Linux += marCCDApp
PROD_IOC_solaris += marCCDApp

LIB_SRCS += marCCD.cpp
LIB_LIBS += NDPlugin ADBase netCDF
LIB_LIBS_Linux += PvAPI ImageLib
LIB_LIBS_WIN32 += PvAPI ImageLib
LIB_LIBS += calc busy sscan autosave mca
LIB_LIBS += asyn
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================

# <name>.dbd will be created from <name>Include.dbd
DBD += marCCDApp.dbd
DBD += marCCD_ADSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
marCCDApp_SRCS_DEFAULT += marCCDApp_registerRecordDeviceDriver.cpp marCCDAppMain.cpp

# Add locally compiled object code
marCCDApp_LIBS += marCCD_AD
marCCDApp_LIBS += NDPlugin ADBase netCDF
marCCDApp_LIBS_Linux += PvAPI ImageLib
marCCDApp_LIBS_WIN32 += PvAPI ImageLib
marCCDApp_LIBS += calc busy sscan autosave mca
marCCDApp_LIBS += asyn
marCCDApp_LIBS += $(EPICS_BASE_IOC_LIBS)

# To use versions of these libraries supplied with areaDetector use this line
marCCDApp_LIBS_Linux        += NeXus mfhdf df hdf5 mxml
# To use system versions of these libraries use this line
#marCCDApp_SYS_LIBS_Linux    += NeXus mfhdf df hdf5 mxml

# To use versions of these libraries supplied with areaDetector use this line
marCCDApp_LIBS_Linux        += tiff jpeg z
# To use system versions of these libraries use this line
#marCCDApp_SYS_LIBS_Linux    += tiff jpeg z

marCCDApp_SYS_LIBS_solaris   += tiff

# use libs supplied with areaDetector (in tiffSupport)
marCCDApp_LIBS_solaris += NeXusCPP NeXus hdf5_cpp hdf5 mxml jpeg sz z

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

