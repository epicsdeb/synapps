#FILENAME...     Makefile
#USAGE...        Makefile for motor application example with Asyn

#Version:        $Revision: 13567 $
#Modified By:    $Author: rivers $
#Last Modified:  $Date: 2011-08-30 14:11:01 -0500 (Tue, 30 Aug 2011) $
#HeadURL:        $URL: https://subversion.xor.aps.anl.gov/synApps/motor/tags/R6-7-1/motorExApp/WithAsyn/Makefile $

# "#!" marks lines that can be uncommented.

TOP = ../..
include $(TOP)/configure/CONFIG

PROD_IOC_DEFAULT = WithAsyn
PROD_IOC_vxWorks = WithAsynVx

COMMONDBDS  = base.dbd
COMMONDBDS += motorSupport.dbd
COMMONDBDS += devAcsMotor.dbd
COMMONDBDS += devImsMotor.dbd
COMMONDBDS += devMclennanMotor.dbd
COMMONDBDS += devMicos.dbd
COMMONDBDS += devMicroMo.dbd
COMMONDBDS += devNewport.dbd
ifdef SNCSEQ
COMMONDBDS += LdevNewportTS.dbd
endif
COMMONDBDS += devPIMotor.dbd
COMMONDBDS += devOms.dbd
COMMONDBDS += devSoftMotor.dbd
COMMONDBDS += motorSimSupport.dbd
COMMONDBDS += devSmartMotorMotor.dbd
COMMONDBDS += devKohzuMotor.dbd
COMMONDBDS += devAttocube.dbd
COMMONDBDS += devAerotech.dbd
COMMONDBDS += ACRMotorSupport.dbd
COMMONDBDS += drvAsynSerialPort.dbd
COMMONDBDS += drvAsynIPPort.dbd
COMMONDBDS += busySupport.dbd

DBD += WithAsyn.dbd
WithAsyn_DBD += $(COMMONDBDS)
ifdef SNCSEQ
WithAsyn_DBD += devSequencer.dbd
endif

DBD += WithAsynVx.dbd
WithAsynVx_DBD += $(COMMONDBDS)
#!WithAsynVx_DBD += drvGsIP488.dbd
ifdef IPAC
WithAsynVx_DBD += MiscVx.dbd
endif

WithAsyn_SRCS   += WithAsyn_registerRecordDeviceDriver.cpp
WithAsyn_SRCS_DEFAULT += WithAsynMain.cpp
WithAsynVx_SRCS += WithAsynVx_registerRecordDeviceDriver.cpp

WithAsyn_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

COMMONLIBS += Aerotech
COMMONLIBS += Attocube
COMMONLIBS += oms
COMMONLIBS += Mclennan
COMMONLIBS += Micos
COMMONLIBS += MicroMo
COMMONLIBS += PI
COMMONLIBS += Acs
COMMONLIBS += Ims
COMMONLIBS += Newport
COMMONLIBS += softMotor
COMMONLIBS += motorSimSupport
COMMONLIBS += SmartMotor
COMMONLIBS += KohzuMotor
COMMONLIBS += ACRMotor
COMMONLIBS += motor

# Needed for Newport SNL programs
WithAsyn_LIBS += $(COMMONLIBS)
WithAsyn_LIBS += asyn
ifdef SNCSEQ
WithAsyn_LIBS += seqDev seq pv
endif
WithAsyn_LIBS += $(EPICS_BASE_IOC_LIBS)

WithAsyn_LIBS += busy

WithAsynVx_LIBS += $(COMMONLIBS)
ifdef IPAC
WithAsynVx_LIBS += Ipac TyGSOctal
endif
WithAsynVx_LIBS += asyn
# Needed for Newport SNL programs
ifdef SNCSEQ
WithAsynVx_LIBS   += seqDev seq pv
endif
WithAsynVx_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES

