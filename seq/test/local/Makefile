TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

# purify support (uncomment to enable)
#CXX := purify $(CXX)

#  Generate snc main programs
SNCFLAGS = +m

#  Products
PROD_HOST = local

#  Libraries
PROD_LIBS += seq 
seq_DIR    = $(INSTALL_LIB)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

