TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
DBD_INSTALLS += softGlueSupport.dbd

INC += drvIP_EP201.h

LIBRARY_IOC_vxWorks = softGlue
LIB_SRCS_vxWorks += drvIP_EP201.c devAsynSoftGlue.c
#LIB_SRCS_vxWorks += sampleCustomInterruptHandler.c

LIB_vxWorks_LIBS += asyn Ipac

LIB_vxWorks_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

