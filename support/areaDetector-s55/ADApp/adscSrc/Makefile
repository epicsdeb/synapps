TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# NOTE: There are lots of lines that are commented out in this Makefile because
# the adsc driver does not yet support reading back the arrays from disk and doing
# callbacks.  Thus, the application does not need to link to the NDPlugin library,
# or any of the libraries that the plugins use.  In the future this may change.

# Set to YES or NO to use or not use the simadsc simulation library instead of
# the real ADSC detcon_lib_th library

USE_SIMADSC = YES

# ADSC control software location (only used if USE_SIMADSC is NO)

ADSC_HOME = /opt/adscdc

ifeq (YES, $(USE_SIMADSC))
  USR_CXXFLAGS += -DUSE_SIMADSC
endif

# The required ADSC header files are detcon_entry.h, detcon_par.h, and
# detcon_state.h.  USR_INCLUDES should include the directories containing
# these header files (only used if USE_SIMADSC is NO).

ifeq (NO, $(USE_SIMADSC))
  USR_INCLUDES += -I$(ADSC_HOME)/src_std/incl
  USR_INCLUDES += -I$(ADSC_HOME)/src_std/detcon_lib_th
endif

# The required ADSC library files are libauxlib.a and libdetcon_th.a.  A
# corresponding <name>_DIR definition is required for each of these and should
# be set to the directory containing the library (only used if USE_SIMADSC is
# NO).

ifeq (NO, $(USE_SIMADSC))
  ifeq (cygwin32, $(OS_CLASS))
    auxlib_DIR    = $(ADSC_HOME)/lib/cygwin
    detcon_th_DIR = $(ADSC_HOME)/lib/cygwin
  endif
  ifeq (Darwin, $(OS_CLASS))
    auxlib_DIR    = $(ADSC_HOME)/lib/darwin
    detcon_th_DIR = $(ADSC_HOME)/lib/darwin
  endif
  ifeq (Linux, $(OS_CLASS))
    auxlib_DIR    = $(ADSC_HOME)/lib/linux
    detcon_th_DIR = $(ADSC_HOME)/lib/linux
  endif
  ifeq (solaris, $(OS_CLASS))
    auxlib_DIR    = $(ADSC_HOME)/lib/sun
    detcon_th_DIR = $(ADSC_HOME)/lib/sun
  endif
endif

LIBRARY_IOC_cygwin32 += adsc
LIBRARY_IOC_Darwin   += adsc
LIBRARY_IOC_Linux    += adsc
LIBRARY_IOC_solaris  += adsc

ifeq (YES, $(USE_SIMADSC))
  SRC_DIRS += ../simadsc
endif

LIB_SRCS += adsc.cpp
ifeq (YES, $(USE_SIMADSC))
  LIB_SRCS += simadsc.c
endif

LIB_LIBS += ADBase
#LIB_LIBS += NDPlugin
#LIB_LIBS_Linux += PvAPI ImageLib
#LIB_LIBS_WIN32 += PvAPI ImageLib
ifeq (NO, $(USE_SIMADSC))
  LIB_LIBS += detcon_th auxlib
endif
LIB_LIBS += busy sscan autosave
LIB_LIBS += asyn
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC_cygwin32 += adscApp
PROD_IOC_Darwin   += adscApp
PROD_IOC_Linux    += adscApp
PROD_IOC_solaris  += adscApp
# <name>.dbd will be created from <name>Include.dbd
DBD += adscApp.dbd
DBD += adscSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
adscApp_SRCS_DEFAULT += adscApp_registerRecordDeviceDriver.cpp adscAppMain.cpp
adscApp_SRCS_vxWorks += adscApp_registerRecordDeviceDriver.cpp

# Add locally compiled object code
adscApp_LIBS += adsc
adscApp_LIBS += ADBase
#adscApp_LIBS += NDPlugin netCDF
#adscApp_LIBS_Linux += PvAPI ImageLib
#adscApp_LIBS_WIN32 += PvAPI ImageLib
ifeq (NO, $(USE_SIMADSC))
  adscApp_LIBS += detcon_th auxlib
endif
adscApp_LIBS += busy sscan autosave 
adscApp_LIBS += asyn
adscApp_LIBS += $(EPICS_BASE_IOC_LIBS)

#PROD_LIBS_cygwin32     += libtiff jpeg libNeXus.dll
PROD_SYS_LIBS_cygwin32 += rpc

# To use versions of these libraries supplied with areaDetector use this line
#PROD_LIBS_Linux        += NeXus mfhdf df hdf5 mxml
# To use system versions of these libraries use this line
#PROD_SYS_LIBS_Linux    += NeXus mfhdf df hdf5 mxml

# To use versions of these libraries supplied with areaDetector use this line
#PROD_LIBS_Linux        += tiff jpeg z
# To use system versions of these libraries use this line
#PROD_SYS_LIBS_Linux    += tiff jpeg z

#PROD_SYS_LIBS_Darwin  += NeXus mfhdf df hdf5 mxml tiff jpeg z

#PROD_LIBS_solaris    += NeXusCPP NeXus hdf5_cpp hdf5 mxml jpeg sz z
#PROD_SYS_LIBS_solaris   += tiff

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

