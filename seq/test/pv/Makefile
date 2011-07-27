TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

# purify support (uncomment to enable)
#CXX := purify $(CXX)

#  Profiling doesn't work for threaded applications?
#arrput_CXXFLAGS = -p
#arrputCA_CXXFLAGS = -p

#  Generate snc main programs
SNCFLAGS = +m

#  Products
#PROD_HOST = testCA pvtest arrput arrputCA
PROD_HOST = pvsimpleCC pvsimpleC pvtest arrput gphtest

#  Libraries
PROD_LIBS += seq
seq_DIR    = $(INSTALL_LIB)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

