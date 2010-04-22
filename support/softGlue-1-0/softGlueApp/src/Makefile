TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
DBD_INSTALLS += softGlueSupport.dbd

LIBRARY_IOC_vxWorks = softGlue
LIB_SRCS_vxWorks += drvIp1k125.c devAsynSoftGlue.c

LIB_vxWorks_LIBS += asyn Ipac

LIB_vxWorks_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

