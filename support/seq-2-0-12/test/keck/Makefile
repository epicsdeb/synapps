TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

# purify support (uncomment to enable)
#CXX := purify $(CXX)

#  Select CA sequencer
USR_CPPFLAGS += -DCA

#  Search for TCS and KNL include files
USR_CPPFLAGS += -I$(KROOT)/rel/dcs/default/include

#  Generate snc main programs
SNCFLAGS = +m

#  Products
PROD_HOST = etcSeq tcsSeq

#  Libraries
PROD_LIBS += seq
seq_DIR    = $(INSTALL_LIB)

#  Extra requirements for tcsSeq
tcsSeq_SRCS = tcsSeq.c pvload.c
tcsSeq_LIBS = tcs
tcs_DIR     = $(KROOT)/rel/dcs/default/lib/$(HOST_ARCH)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

