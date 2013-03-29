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
TESTPROD_HOST += arrput
TESTPROD_HOST += arrputCA
TESTPROD_HOST += pvsimpleC
TESTPROD_HOST += pvsimpleCC
TESTPROD_HOST += pvtest

#  Libraries
PROD_LIBS += seq pv
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
