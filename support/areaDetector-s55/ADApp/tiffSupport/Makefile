TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# This directory contains files to support libtiff and libjpeg on WIN32, so users don't need to install it.
# These files were obtained from the distribution at
# http://gnuwin32.sourceforge.net/packages/tiff.htm
# http://gnuwin32.sourceforge.net/packages/jpeg.htm
# 
INC_WIN32    += tiff.h tiffio.h tiffvers.h tiffconf.h 
INC_cygwin32 += tiff.h tiffio.h tiffvers.h tiffconf.h 
INC_WIN32    += jpeglib.h jconfig.h jmorecfg.h jerror.h 
INC_cygwin32 += jpeglib.h jconfig.h jmorecfg.h jerror.h 
# Use these lines to install local versions of these libraries
# Comment them out to use system versions of the libraries
INC_Linux    += tiff.h tiffio.h tiffvers.h tiffconf.h tiffconf-32.h tiffconf-64.h 
INC_Linux    += jpeglib.h jconfig.h jmorecfg.h jerror.h 
INC_solaris  += jconfig.h jerror.h jmorecfg.h jpeglib.h szlib.h zconf.h zlib.h

LIB_INSTALLS_WIN32    += ../libtiff.lib  ../jpeg.lib
LIB_INSTALLS_cygwin32 += ../libtiff.lib  ../jpeg.lib
LIB_INSTALLS_solaris += ../os/solaris-sparc/libjpeg.a ../os/solaris-sparc/libsz.a ../os/solaris-sparc/libz.a
# Use this line to install local versions of these libraries
# Comment it out to use system versions of the libraries
ifeq ($(T_A), linux-x86)
LIB_INSTALLS_Linux += ../os/linux-x86/libtiff.a    ../os/linux-x86/libjpeg.a    ../os/linux-x86/libz.a
endif
ifeq ($(T_A), linux-x86_64) 
LIB_INSTALLS_Linux += ../os/linux-x86_64/libtiff.a ../os/linux-x86_64/libjpeg.a ../os/linux-x86_64/libz.a
endif
BIN_INSTALLS_WIN32    += ../libtiff3.dll ../jpeg62.dll ../zlib1.dll
BIN_INSTALLS_cygwin32 += ../libtiff3.dll ../jpeg62.dll ../zlib1.dll

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

