TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

SNC = $(INSTALL_HOST_BIN)/snc$(HOSTEXE)

# purify support (uncomment to enable)
#CXX := purify $(CXX)

#  Select KTL sequencer
#USR_CPPFLAGS += -DKTL

#  Generate snc main programs (not needed under VxWorks)
SNCFLAGS_DEFAULT += +m
SNCFLAGS_vxWorks += -nil-

#  Products
PROD = johng

#  Libraries
PROD_LIBS += seq pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
