#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure vacApp iocBoot
vacApp_DEPEND_DIRS  = configure
iocBoot_DEPEND_DIRS = vacApp

include $(TOP)/configure/RULES_TOP
