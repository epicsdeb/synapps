TOP = ..
include $(TOP)/configure/CONFIG

DIRS += ccdSrc
DIRS += marCCDSrc
marCCDSrc_DEPEND_DIRS  = ccdSrc
DIRS += marIpCBFSrc
DIRS += marIpIoSrc
marIpIoSrc_DEPEND_DIRS = marIpCBFSrc
DIRS += marIpSrc
marIpSrc_DEPEND_DIRS   = marIpIoSrc
DIRS += brukerSrc
brukerSrc_DEPEND_DIRS  = ccdSrc
DIRS += roperSrc
roperSrc_DEPEND_DIRS   = ccdSrc

DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
include $(TOP)/configure/RULES_DIRS

