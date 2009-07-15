TOP=../..


include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_LDFLAGS += -L../../../lib/$(T_A)
USR_INCLUDES += -I$(EPICS_BASE)/src/rsrv
USR_INCLUDES += -I$(EPICS_BASE)/src/ca
#=======Create Host Programs=======
ca_DIR = $(EPICS_BASE_LIB)
Com_DIR = $(EPICS_BASE_LIB)

#USR_CFLAGS +=

# msi exits apparently with arg error.  Try not defining this
###DBEXPAND = msi

# For Each Host Product define the following:1
#PROD_HOST = <product>
#<product>_SRCS += xxx.c 
#...
#<product>_LIBS += ca
#<product>_LIBS += Com

PROD_HOST_DEFAULT += ioc_stats
PROD_HOST_WIN32 += -nil-
ioc_stats_SRCS += ioc_stats.c
ioc_stats_SRCS += BSlib.c
ioc_stats_LIBS_solaris += $(EPICS_BASE_HOST_LIBS) socket nsl
socket_DIR = /usr/lib
nsl_DIR = /usr/lib

PROD_HOST_DEFAULT += rdbls
PROD_HOST_WIN32 += -nil-
rdbls_SRCS += rdbls.c
rdbls_SRCS += BSlib.c
rdbls_LIBS_solaris += $(EPICS_BASE_HOST_LIBS) socket nsl

PROD_HOST_DEFAULT += rdbapplist
PROD_HOST_WIN32 += -nil-
rdbapplist_SRCS += rdbapplist.c
rdbapplist_SRCS += BSlib.c
rdbapplist_LIBS_solaris += $(EPICS_BASE_HOST_LIBS) socket nsl


#========Database Products================

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord

#<name>.dbd will be created from <name>Include.dbd
#DBD += <name>.dbd

DBD += vxStats.dbd
#=======Create an IOC program=====

#PROD_IOC = <name>

#<name>_SRCS += xxx.c # .cpp .stt etc
#...
#<name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
#<name>_SRCS += <name>_registerRecordDeviceDriver.cpp

## If <name> is to be built for both vxWorks and for the host then
#<name>_SRCS_DEFAULT += <name>Main.cpp
#<name>_SRCS_vxWorks += -nil-

#The following adds support from base/src/vxWorks
#<name>_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

##Define all possible libraries
## Use win32 object libs for registered support
#<name>_LIBS_WIN32 += recIocObj
#<name>_LIBS_WIN32 += softDevIocObj
#<name>_LIBS_WIN32 += testDevIocObj
#<name>_LIBS_DEFAULT += recIoc
#<name>_LIBS_DEFAULT += softDevIoc
#<name>_LIBS_DEFAULT += testDevIoc

#<name>_LIBS += iocsh
#<name>_LIBS += miscIoc
#<name>_LIBS += rsrvIoc
#<name>_LIBS += dbtoolsIoc
#<name>_LIBS += asIoc
#<name>_LIBS += dbIoc
#<name>_LIBS += registryIoc
#<name>_LIBS += dbStaticIoc
#<name>_LIBS += ca
#<name>_LIBS += Com

## If sequence programs are being built
#<name>_LIBS += seq
#<name>_LIBS += pv
#seq_DIR    = $(SNCSEQ_LIB)
#pv_DIR     = $(SNCSEQ_LIB)



LIBRARY_IOC_vxWorks = vxStats
vxStats_SRCS += devVxStats.c
vxStats_SRCS += read_ca_stats.c
vxStats_SRCS += BSlib.c
vxStats_SRCS += reboot.c
vxStats_OBJ_vxWorks += \$(EPICS_BASE_BIN)/vxComLibrary
vxStats_LIBS_solaris += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

