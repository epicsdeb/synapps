TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# xxxRecord.h will be created from xxxRecord.dbd

DBDINC += vmeRecord

# <name>.dbd will be created from <name>Include.dbd
DBD += vmeSupport.dbd
DBD += vmeVXSupport.dbd

#=============================

LIBRARY_IOC += vme

vme_SRCS_vxWorks += devAiVaroc.c drvVarocB.c
vme_SRCS_vxWorks += devXxHP10895LaserAxis.c

# Heidenhain IK320 support
vme_SRCS_vxWorks += devIK320.c
vme_SRCS_vxWorks += drvIK320.c
vme_SRCS_vxWorks += drvIK320ErrStat.c

#vme_SRCS_vxWorks += devScaler.c
vme_SRCS_vxWorks += devScaler_debug.c
vme_SRCS_vxWorks += devScaler_VS.c
vme_SRCS_vxWorks += vmeRecord.c
vme_SRCS_vxWorks += devBunchClkGen.c
vme_SRCS_vxWorks += devAoVMI4116.c
vme_SRCS_vxWorks += devAvme9440.c
vme_SRCS_vxWorks += devAoAvme9210.c
vme_SRCS_vxWorks += drvAvme9210.c

# For generic VME A32/D32 access
vme_SRCS_vxWorks += devA32Vme.c

# For Machine Status Link MRD-100 board
vme_SRCS_vxWorks += devAvmeMRD.c

# For general test of VME modules
vme_SRCS_vxWorks += vmeTest.c

# The following adds support from base/src/vxWorks
### The following builds SNL programs as components of vme
# NOTE: for suffix .st, code is run through cpp; for .stt, it's not
vme_SRCS += getFilledBuckets.st

vme_LIBS += seq pv
seq_DIR   = $(SNCSEQ_LIB)
pv_DIR    = $(SNCSEQ_LIB)

vme_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

