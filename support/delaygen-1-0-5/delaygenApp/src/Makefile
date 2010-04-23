TOP=../..

include $(TOP)/configure/CONFIG
#-----------------------------------------------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================================================================

#=============================================================================
# Build an IOC support library
LIBRARY_IOC += delaygen

delaygen_SRCS_Linux += nigpibInterpose.c
delaygen_SRCS_Linux += drvAsynDG645.cpp
delaygen_SRCS_Linux += drvAsynColby.cpp
delaygen_SRCS_Linux += digitalDelay.c
delaygen_SRCS_Linux += devDG535.c
delaygen_LIBS_Linux += std asyn
delaygen_LIBS_Linux += $(EPICS_BASE_IOC_LIBS)

delaygen_SRCS_RTEMS += nigpibInterpose.c
delaygen_SRCS_RTEMS += drvAsynDG645.cpp
delaygen_SRCS_RTEMS += drvAsynColby.cpp
delaygen_SRCS_RTEMS += digitalDelay.c
delaygen_SRCS_RTEMS += devDG535.c
delaygen_LIBS_RTEMS += std asyn
delaygen_LIBS_RTEMS += $(EPICS_BASE_IOC_LIBS)

delaygen_SRCS_vxWorks += nigpibInterpose.c
delaygen_SRCS_vxWorks += drvAsynDG645.cpp
delaygen_SRCS_vxWorks += drvAsynColby.cpp
delaygen_SRCS_vxWorks += digitalDelay.c
delaygen_SRCS_vxWorks += devDG535.c
delaygen_LIBS_vxWorks += std asyn TyGSOctal Ipac ip
delaygen_LIBS_vxWorks += $(EPICS_BASE_IOC_LIBS)

#=============================================================================

DBD += devDG535.dbd
DBD += drvAsynDG645.dbd
DBD += drvAsynColby.dbd
DBD += digitalDelay.dbd
DBD += nigpibSupport.dbd
DBD += delaygenSupport.dbd

#
#=============================================================================

include $(TOP)/configure/RULES
#-----------------------------------------------------------------------------
#  ADD RULES AFTER THIS LINE
