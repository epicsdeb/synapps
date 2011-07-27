TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc

#  Generate snc main programs (not needed under VxWorks)
SNCFLAGS_DEFAULT += +m
SNCFLAGS_vxWorks += -nil-

#  Sequence programs and/or object files to create
PROD += sncDelay
PROD += sncEntry
PROD += sncEntryOpte
PROD += sncEntryVar
PROD += sncExitOptx
PROD += sncOptt
PROD += sncOpttVar

# this test is supposed to fail on 64 bit systems
#PROD += tooLong

#  Libraries
PROD_LIBS += seq
PROD_LIBS += pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
