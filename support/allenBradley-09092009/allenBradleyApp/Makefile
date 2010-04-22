#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
TOP = ..
include $(TOP)/configure/CONFIG

AB = $(TOP)/allenBradleyApp
#USR_CFLAGS += -DDEBUG

SRC_DIRS += $(AB)/1746HSTP1Src
SRC_DIRS += $(AB)/1771DCMSrc
SRC_DIRS += $(AB)/1771IFESrc
SRC_DIRS += $(AB)/1771IXSrc
SRC_DIRS += $(AB)/1771NSeriesSrc
SRC_DIRS += $(AB)/1791BlockIOSrc
SRC_DIRS += $(AB)/SLCDCMSrc
SRC_DIRS += $(AB)/basicSrc
SRC_DIRS += $(AB)/gpIfaceSrc
SRC_DIRS += $(AB)/oldSrc

LIBRARY_IOC_vxWorks += allenBradley
allenBradley_LIBS += $(EPICS_BASE_IOC_LIBS)

INC += abDcm.h
INC += devIai1.h
INC += devIao1.h
INC += drvAb.h

DBDINC += ab1771IFERecord
DBDINC += ab1771IXRecord
DBDINC += ab1771NRecord
DBDINC += ab1791Record
DBDINC += abDcmRecord

DBD += allenBradley.dbd

#If you want to build devSmAB1746HSTP1 RELEASE must include
#support for the steppermotorRecord
#allenBradley_SRCS += devSmAB1746HSTP1.c

allenBradley_SRCS += abDcmRecord.c
allenBradley_SRCS += devAiAbDcm.c
allenBradley_SRCS += devAoAbDcm.c
allenBradley_SRCS += devBiAbDcm.c
allenBradley_SRCS += devBoAbDcm.c
allenBradley_SRCS += devLiAbDcm.c
allenBradley_SRCS += devLoAbDcm.c
allenBradley_SRCS += devMbbiAbDcm.c
allenBradley_SRCS += devMbboAbDcm.c
allenBradley_SRCS += plcSimulate.c

allenBradley_SRCS += ab1771IFERecord.c

allenBradley_SRCS += ab1771IXRecord.c

allenBradley_SRCS += ab1771NRecord.c

allenBradley_SRCS += ab1791Record.c

allenBradley_SRCS += devABSLCDCM.c

allenBradley_SRCS += drvAb.c
allenBradley_SRCS += devABBINARY.c
allenBradley_SRCS += devABStatus.c

allenBradley_SRCS += devIai1.c
allenBradley_SRCS += devIao1.c

allenBradley_SRCS += devAB1771IFE.c
allenBradley_SRCS += devAB1771IL.c
allenBradley_SRCS += devAB1771IR.c
allenBradley_SRCS += devAB1771IXE.c
allenBradley_SRCS += devAB1771OFE.c

include $(TOP)/configure/RULES
