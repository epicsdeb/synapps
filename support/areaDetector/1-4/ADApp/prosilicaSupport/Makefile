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
LIB_INSTALLS_Linux    += ../libPvAPI.a ../libImageLib.a

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

