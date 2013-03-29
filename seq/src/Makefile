TOP = ..
include $(TOP)/configure/CONFIG

DIRS += pv

DIRS += seq
seq_DEPEND_DIRS = pv

DIRS += lemon

DIRS += dev
dev_DEPEND_DIRS = seq

DIRS += snc
snc_DEPEND_DIRS += seq
snc_DEPEND_DIRS += lemon

include $(TOP)/configure/RULES_DIRS
