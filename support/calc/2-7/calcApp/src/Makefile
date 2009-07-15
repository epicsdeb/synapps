TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# xxxRecord.h will be created from xxxRecord.dbd

DBDINC += transformRecord
DBDINC += sCalcoutRecord
DBDINC += aCalcoutRecord
ifdef SSCAN
	DBDINC += swaitRecord
endif

# <name>.dbd will be created from <name>Include.dbd, if it exists
DBD_INSTALLS += calcSupport.dbd

#=============================

LIBRARY_IOC += calc

calc_SRCS += transformRecord.c
calc_SRCS += sCalcPostfix.c sCalcPerform.c
calc_SRCS += aCalcPostfix.c aCalcPerform.c calcUtil.c
#calc_SRCS_vxWorks += test_sCalc.c
calc_SRCS += sCalcoutRecord.c devsCalcoutSoft.c
calc_SRCS += aCalcoutRecord.c devaCalcoutSoft.c
calc_SRCS += subAve.c

ifdef SSCAN
	calc_SRCS += swaitRecord.c
	calc_LIBS += sscan
endif

calc_SRCS += interp.c
calc_SRCS += arrayTest.c

# Register stuff
calc_SRCS += calcRegister.c

calc_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application for testing

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# calcSupport.dbd is for use by an ioc directory that needs stuff from the
# calc module. We build this by hand so we can make it depend on the modules
# defined in configure/RELEASE and also leave it unexpanded.  (We can't
# expand, because that would require including base.dbd, and support modules
# mustn't do that.  Otherwise the ioc directory would inherit an expanded copy
# of base.dbd from each support module.)

ifdef SSCAN
calcSupport.dbd: ../calcSupport_LOCAL.dbd ../calcSupport_withSSCAN.dbd ../calcSupport_withGENSUB.dbd
	perl ../cat.pl ../calcSupport_LOCAL.dbd ../calcSupport_withSSCAN.dbd ../calcSupport_withGENSUB.dbd calcSupport.dbd
else
calcSupport.dbd: ../calcSupport_LOCAL.dbd ../calcSupport_withGENSUB.dbd
	perl ../cat.pl ../calcSupport_LOCAL.dbd ../calcSupport_withGENSUB.dbd calcSupport.dbd
endif
