TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

INC += napi.h

# Note, the following files were manually copied from the C:\Program Files\NeXus Data Format\bin and \lib
# directories after a binary install on Windows
BIN_INSTALLS_WIN32    += ../libNeXus-0.dll ../hd421m.dll ../hm421m.dll ../szlibdll.dll ../hdf5dll.dll
BIN_INSTALLS_cygwin32 += ../libNeXus-0.dll ../hd421m.dll ../hm421m.dll ../szlibdll.dll ../hdf5dll.dll
LIB_INSTALLS_WIN32    += ../libNeXus.dll.lib
LIB_INSTALLS_cygwin32 += ../libNeXus.dll.lib
ifeq ($(T_A), linux-x86)
LIB_INSTALLS_Linux     += ../os/linux-x86/libNeXus.a    ../os/linux-x86/libhdf5.a    ../os/linux-x86/libdf.a    ../os/linux-x86/libmfhdf.a    ../os/linux-x86/libmxml.a
endif
ifeq ($(T_A), linux-x86_64)
LIB_INSTALLS_Linux     += ../os/linux-x86_64/libNeXus.a ../os/linux-x86_64/libhdf5.a ../os/linux-x86_64/libdf.a ../os/linux-x86_64/libmfhdf.a ../os/linux-x86_64/libmxml.a
endif

SOLARIS-SPARC-LIBS = $(addprefix ../os/solaris-sparc/, libNeXus.a libNeXusCPP.a libhdf5.a libhdf5_cpp.a libmxml.a)

ifeq (solaris-sparc, $(findstring solaris-sparc, $(T_A)))
LIB_INSTALLS_solaris += $(SOLARIS-SPARC-LIBS)
endif
#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

