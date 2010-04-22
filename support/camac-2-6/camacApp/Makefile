TOP = ..
include $(TOP)/configure/CONFIG
DIRS += src
DIRS += devsrc
devsrc_DEPEND_DIRS = src
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
include $(TOP)/configure/RULES_DIRS

