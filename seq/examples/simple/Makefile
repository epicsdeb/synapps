TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc$(HOSTEXE)

#  Generate snc main programs
SNCFLAGS_DEFAULT += +m
SNCFLAGS_vxWorks += -nil-

#  Sequence programs to create
PROD_HOST += sncExample
PROD_HOST += sncExEntry
PROD_HOST += sncExOpt

#  Libraries
PROD_LIBS += seq pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
