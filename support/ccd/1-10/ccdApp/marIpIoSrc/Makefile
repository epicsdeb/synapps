TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CFLAGS += 
#USR_CXXFLAGS +=

INC += mar345_header.h
INC += mar300_header.h
INC += nb_header.h

LIBRARY_IOC_DEFAULT += marIpIo
LIBRARY_IOC_WIN32   += -nil-
LIB_SRCS += mar300_header.c mar345_header.c swap.c utils.c nb_header.c marcif.c
LIB_LIBS += marIpCBF

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

