#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure ipApp iocBoot
ipApp_DEPEND_DIRS   = configure
iocBoot_DEPEND_DIRS = ipApp
include $(TOP)/configure/RULES_TOP
