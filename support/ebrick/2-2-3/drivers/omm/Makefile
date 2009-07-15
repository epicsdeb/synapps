UNAME := $(shell uname -r)
KERNEL26 := 2.6
KERNELVERSION := $(findstring $(KERNEL26),$(UNAME))

all:: ommtest

ifeq ($(KERNELVERSION),2.6)

obj-m	:= omm.o

INCLUDE	:= -I/usr/include/asm/mach-default/
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD		:= $(shell pwd)

all::
	$(MAKE) -C $(KDIR) $(INCLUDE) SUBDIRS=$(PWD) modules
	
else

TARGET	:= omm
INCLUDE	:= -I/lib/modules/`uname -r`/build/include -I/usr/include/asm/mach-default/
CFLAGS	:= -O2 -Wall -DMODULE -D__KERNEL__ -DLINUX
CC	:= gcc

all:: ${TARGET}.o

${TARGET}.o: ${TARGET}.c
	$(CC) $(CFLAGS) ${INCLUDE} -c ${TARGET}.c
	
endif

ommtest:  ommtest.c ommmon
	gcc -o $@ $<

ommmon:   ommmon.c
	gcc -o $@ $<

clean:
	rm -f omm.o omm.ko omm.mod.* .omm.*.cmd ommtest ommmon

