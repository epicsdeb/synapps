TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC_vxWorks += camac

DBDINC += camacRecord
DBD += camacSupport.dbd
USR_CFLAGS = -DHW_HEADER="<ksc2917.h>"

# The following are compiled and added to the Support library
camac_SRCS += camacRecord.c
camac_SRCS += camacLib.c

camac_LIBS += $(EPICS_BASE_IOC_LIBS)

INC += camacLib.h

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

