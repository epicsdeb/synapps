TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# The files in this directory are installed first because they are
# used both by the NDPluginColorConvert and by the Prosilica driver
INC += PvApi.h ImageLib.h

# Note, the following files were manually copied from the Prosilica GigESDK/bin-pc directory
# to this directory after building
BIN_INSTALLS_WIN32    += ../PvAPI.dll
BIN_INSTALLS_cygwin32 += ../PvAPI.dll
LIB_INSTALLS_WIN32    += ../PvAPI.lib ../ImageLib.lib
LIB_INSTALLS_cygwin32 += ../PvAPI.lib ../ImageLib.lib
ifeq ($(T_A), linux-x86)
LIB_INSTALLS_Linux += ../os/linux-x86/libPvAPI.a    ../os/linux-x86/libImageLib.a
endif
ifeq ($(T_A), linux-x86_64) 
LIB_INSTALLS_Linux += ../os/linux-x86_64/libPvAPI.a ../os/linux-x86_64/libImageLib.a
endif

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

