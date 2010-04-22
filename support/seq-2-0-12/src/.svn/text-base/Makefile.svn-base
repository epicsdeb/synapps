TOP = ..
include $(TOP)/configure/CONFIG

DIRS += pv

DIRS += seq
seq_DEPEND_DIRS = pv

DIRS += dev
dev_DEPEND_DIRS = seq

DIRS += snc
snc_DEPEND_DIRS = seq

include $(TOP)/configure/RULES_DIRS
