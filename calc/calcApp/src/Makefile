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
DBDINC += sseqRecord
ifdef SSCAN
	DBDINC += swaitRecord
endif

# <name>.dbd will be created from <name>Include.dbd, if it exists
DBD_INSTALLS += calcSupport.dbd

#=============================

LIBRARY_IOC += calc

calc_SRCS += transformRecord.c
calc_SRCS += sCalcPostfix.c sCalcPerform.c
calc_SRCS += aCalcPostfix.c aCalcPerform.c calcUtil.c myFreeListLib.c
#calc_SRCS_vxWorks += test_sCalc.c
calc_SRCS += sCalcoutRecord.c devsCalcoutSoft.c
calc_SRCS += aCalcoutRecord.c devaCalcoutSoft.c
calc_SRCS += sseqRecord.c
calc_SRCS += subAve.c

ifdef SSCAN
	calc_SRCS += swaitRecord.c
	calc_LIBS += sscan
endif

calc_SRCS += interp.c
calc_SRCS += arrayTest.c

calc_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application for testing

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# Parallel build sometimes fails.
# Make dependences on new records explicit.
aCalcoutRecord$(OBJ): $(COMMON_DIR)/aCalcoutRecord.h
devaCalcoutSoft$(OBJ): $(COMMON_DIR)/aCalcoutRecord.h
transformRecord$(OBJ): $(COMMON_DIR)/transformRecord.h
sseqRecord$(OBJ): $(COMMON_DIR)/sseqRecord.h
sCalcoutRecord$(OBJ): $(COMMON_DIR)/sCalcoutRecord.h
devsCalcoutSoft$(OBJ): $(COMMON_DIR)/sCalcoutRecord.h
swaitRecord$(OBJ): $(COMMON_DIR)/swaitRecord.h

# calcSupport.dbd is for use by an ioc directory that needs stuff from the
# calc module. We build this by hand so we can make it depend on the modules
# defined in configure/RELEASE and also leave it unexpanded.  (We can't
# expand, because that would require including base.dbd, and support modules
# mustn't do that.  Otherwise the ioc directory would inherit an expanded copy
# of base.dbd from each support module.)

ifdef SSCAN
calcSupport.dbd: ../calcSupport_LOCAL.dbd ../calcSupport_withSSCAN.dbd
	$(PERL) -MExtUtils::Command -e cat $^ > $@
	#perl ../cat.pl ../calcSupport_LOCAL.dbd ../calcSupport_withSSCAN.dbd calcSupport.dbd
else
calcSupport.dbd: ../calcSupport_LOCAL.dbd
	$(PERL) -MExtUtils::Command -e cat $^ > $@
	#perl ../cat.pl ../calcSupport_LOCAL.dbd calcSupport.dbd
endif
