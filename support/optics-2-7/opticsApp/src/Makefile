TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#PROD_HOST += caExample
#caExample_SRCS	+= caExample.c
#caExample_LIBS	+= ca
#caExample_LIBS	+= Com
#ca_DIR           = $(EPICS_BASE_LIB)
#Com_DIR          = $(EPICS_BASE_LIB)


#=============================
# xxxRecord.h will be created from xxxRecord.dbd

DBDINC += tableRecord

DBD += opticsSupport.dbd

#=============================

LIBRARY_IOC += optics


optics_SRCS += tableRecord.c

### The following builds SNL programs as components of optics
# NOTE: for suffix .st, code is run through cpp; for .stt, it's not
optics_SRCS += kohzuCtl.st
optics_SRCS += hrCtl.st
optics_SRCS += ml_monoCtl.st

# Pete Jemian's support for X-ray Instrumentation Associates
# HSC-1 slit controller (SNL program)
optics_SRCS   += xiahsc.st
# customized version that works with sscan record
optics_SRCS   += xia_slit.st

# Jon Tischler's ionization-chamber calc
optics_SRCS   += Io.st

# Support for orientation matrix, four-circle diffractometer
optics_SRCS   += matrix3.c, orient.c, orient_st.st

# Support for XIA PF4 filter (dual)
optics_SNCFLAGS += +d -c +r -w
optics_SRCS += pf4.st

# Pete Jemian's (SNL program) support for
#    Oxford Quad X-ray Beam Position Monitor
optics_SRCS   += sncqxbpm.st
 
optics_LIBS += seq pv
optics_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

