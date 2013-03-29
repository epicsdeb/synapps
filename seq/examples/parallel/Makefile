TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Location of SNL compiler
SNC = $(INSTALL_HOST_BIN)/snc$(HOSTEXE)

#  Generate snc main program
SNCFLAGS = +m

#  Products
PROD_HOST = control

#  Libraries
PROD_LIBS += seq pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
