UNAME := $(shell uname -r)
KERNEL26 := 2.6
KERNELVERSION := $(findstring $(KERNEL26),$(UNAME))

all:: astest

ifeq ($(KERNELVERSION),2.6)

obj-m	:= autoshut.o

INCLUDE	:= -I/usr/include/asm/mach-default/
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD		:= $(shell pwd)

all::
	$(MAKE) -C $(KDIR) $(INCLUDE) SUBDIRS=$(PWD) modules
	
else

TARGET	:= autoshut
INCLUDE	:= -I/lib/modules/`uname -r`/build/include -I/usr/include/asm/mach-default/
CFLAGS	:= -O2 -Wall -DMODULE -D__KERNEL__ -DLINUX
CC	:= gcc

all:: ${TARGET}.o

${TARGET}.o: ${TARGET}.c
	$(CC) $(CFLAGS) ${INCLUDE} -c ${TARGET}.c
	
endif

astest:  astest.c asmon
	gcc -o $@ $<

asmon:  asmon.c
	gcc -o $@ $<

clean:
	rm -f autoshut.o autoshut.ko autoshut.mod.* .autoshut.*.cmd astest asmon

