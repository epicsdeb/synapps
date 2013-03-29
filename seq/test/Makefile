TOP = ..
include $(TOP)/configure/CONFIG

DIRS += compiler
DIRS += pv
DIRS += validate

ifeq '$(EPICS_HAS_UNIT_TEST)' '1'
DIRS += unit
endif

include $(TOP)/configure/RULES_DIRS
