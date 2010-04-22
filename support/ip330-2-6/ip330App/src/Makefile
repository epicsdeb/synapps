TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

LIBRARY_IOC_vxWorks += ip330
LIBRARY_IOC_RTEMS   += ip330

# If your Linux system supports drvIpac and devLib then uncomment this line
LIBRARY_IOC_Linux += ip330

ip330_SRCS += drvIp330.c

INC += drvIp330.h
DBD += ip330Support.dbd

ip330_LIBS += $(EPICS_BASE_IOC_LIBS)
#=============================


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
