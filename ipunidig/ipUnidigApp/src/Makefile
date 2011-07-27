TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

LIBRARY_IOC_vxWorks += ipUnidig
LIBRARY_IOC_RTEMS += ipUnidig
# If your Linux system supports drvIpac and devLib then uncomment this line
LIBRARY_IOC_Linux += ipUnidig

DBD += ipUnidigSupport.dbd

ipUnidig_SRCS += drvIpUnidig.cpp

ipUnidig_LIBS += $(EPICS_BASE_IOC_LIBS)
#=============================


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
