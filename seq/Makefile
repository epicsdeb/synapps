# Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure

DIRS += src
src_DEPEND_DIRS  = configure

DIRS += test
test_DEPEND_DIRS = src

include $(TOP)/configure/RULES_TOP

upload:
	darcs push wwwcsr@www-csr.bessy.de:www/control/SoftDist/sequencer/branch-2-0-13
	darcs dist -d seq-2.0.13
	rsync seq-2.0.13.tar.gz wwwcsr@www-csr.bessy.de:www/control/SoftDist/sequencer/releases/

.PHONY: upload
