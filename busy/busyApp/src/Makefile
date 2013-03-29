TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
DBD_INSTALLS += busySupport.dbd
DBDINC += busyRecord

LIBRARY_IOC = busy
LIB_SRCS += busyRecord.c
LIB_SRCS += devBusySoft.c
LIB_SRCS += devBusySoftRaw.c

ifdef ASYN
	LIB_SRCS += devBusyAsyn.c
	LIB_LIBS += asyn
endif
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

ifdef ASYN
busySupport.dbd: ../busySupport_LOCAL.dbd ../busySupport_withASYN.dbd
	perl ../cat.pl ../busySupport_LOCAL.dbd ../busySupport_withASYN.dbd  busySupport.dbd
else
busySupport.dbd: ../busySupport_LOCAL.dbd
	perl ../cat.pl ../busySupport_LOCAL.dbd busySupport.dbd
endif

# Parallel build sometimes fails.
# Make dependences on busyRecord.h explicit.
busyRecord$(OBJ):     $(COMMON_DIR)/busyRecord.h
devBusySoft$(OBJ):    $(COMMON_DIR)/busyRecord.h
devBusySoftRaw$(OBJ): $(COMMON_DIR)/busyRecord.h
devBusyAsyn$(OBJ):    $(COMMON_DIR)/busyRecord.h

