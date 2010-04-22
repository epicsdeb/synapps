TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc

#  Generate snc main programs
SNCFLAGS_DEFAULT += +m
SNCFLAGS_vxWorks += -nil-

#  Sequence programs and/or object files to create
SEQS = sncExample sncExEntry sncExOpt
PROD_HOST = $(SEQS)
sncExample_SRCS = sncExample.st
sncExEntry_SRCS = sncExEntry.st
sncExOpt_SRCS = sncExOpt.st

#  Libraries
PROD_LIBS += seq
PROD_LIBS += pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

