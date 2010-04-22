TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# The files in this directory are installed first because they are
# used by PVCam driver

INC += pvcam.h pv_icl.h master.h

# Note, the following files were manually copied from the PVCam distribution
# to this directory after building
BIN_INSTALLS_WIN32    += ../pv_icl32.dll ../pvcam32.dll
BIN_INSTALLS_cygwin32 += ../pv_icl32.dll ../pvcam32.dll
LIB_INSTALLS_WIN32    += ../pv_icl32.lib ../pvcam32.lib
LIB_INSTALLS_cygwin32 += ../pv_icl32.lib ../pvcam32.lib

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

