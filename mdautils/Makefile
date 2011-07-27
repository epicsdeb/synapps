OSTYPE := $(shell uname -s)

ifeq ($(OSTYPE),SunOS)
  EXT_LIB=-lnsl
else
  EXT_LIB=
endif

ifeq ($(OSTYPE),Darwin)
  EXT_CFLAGS=-D DARWIN
else
  EXT_CFLAGS=
endif

CC = gcc
AR = ar
CFLAGS = -Wall -O2 $(EXT_CFLAGS)
#CFLAGS += -g

TARGETS = libmda-load.a mda2ascii mda-dump mda-info mda-ls 


all: $(TARGETS)

libmda-load.a: mda-load.h mda_loader.o
	$(AR) rcs libmda-load.a mda_loader.o

mda2ascii: mda_ascii.o mda_loader.o mda-load.h
	$(CC) mda_ascii.o mda_loader.o -o mda2ascii $(EXT_LIB)

mda-dump: mda_dump.o
	$(CC) mda_dump.o -o mda-dump $(EXT_LIB)

mda-info: mda_info.o mda_loader.o mda-load.h
	$(CC) mda_info.o mda_loader.o -o mda-info $(EXT_LIB)

mda-ls: mda_ls.o mda_loader.o mda-load.h
	$(CC) mda_ls.o mda_loader.o -o mda-ls $(EXT_LIB)


mda_dump.o:
mda_loader.o: mda-load.h
mda_ascii.o:  mda-load.h
mda_info.o:   mda-load.h
mda_ls.o:     mda-load.h


.PHONY : clean
clean:
	-rm *.o $(TARGETS)


prefix     = /usr/local
bindir     = $(prefix)/bin
mandir     = $(prefix)/man/man1
includedir = $(prefix)/include
libdir     = $(prefix)/lib

INSTALL_MKDIR = mkdir -p
INSTALL_EXE   = install -c -m 0755
INSTALL_OTHER = install -c -m 0644

install : all
	$(INSTALL_MKDIR) $(bindir)
	$(INSTALL_MKDIR) $(libdir)
	$(INSTALL_MKDIR) $(includedir)
	$(INSTALL_MKDIR) $(mandir)
	$(INSTALL_EXE) mda-ls $(bindir)/
	$(INSTALL_EXE) mda-info $(bindir)/
	$(INSTALL_EXE) mda-dump $(bindir)/
	$(INSTALL_EXE) mda2ascii $(bindir)/
	$(INSTALL_OTHER) libmda-load.a $(libdir)/
	$(INSTALL_OTHER) mda-load.h $(includedir)/
	$(INSTALL_OTHER) doc/mda-ls.1 $(mandir)/
	$(INSTALL_OTHER) doc/mda-info.1 $(mandir)/
	$(INSTALL_OTHER) doc/mda-dump.1 $(mandir)/
	$(INSTALL_OTHER) doc/mda2ascii.1 $(mandir)/

