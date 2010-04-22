TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc

#  Generate snc main programs (not needed under VxWorks)
SNCFLAGS_DEFAULT += +m
SNCFLAGS_vxWorks += -nil-

#  Sequence programs and/or object files to create
SEQS = sncDelay sncEntry sncEntryOpte sncEntryVar sncExitOptx sncOptt sncOpttVar
PROD_HOST = $(SEQS)

sncDelay_SRCS = sncDelay.st
sncEntry_SRCS = sncEntry.st
sncEntryOpte_SRCS = sncEntryOpte.st
sncEntryVar_SRCS = sncEntryVar.st
sncExitOptx_SRCS = sncExitOptx.st
sncOptt_SRCS = sncOptt.st
sncOpttVar_SRCS = sncOpttVar.st

#  Libraries
PROD_LIBS += seq
PROD_LIBS += pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

