UNAME := $(shell uname -r)
KERNEL26 := 2.6
KERNELVERSION := $(findstring $(KERNEL26),$(UNAME))

all:: read write serialink writeread writeread2

read:  read.c
	gcc -o $@ $<

write:  write.c
	gcc -o $@ $<

serialink:  serialink.c
	gcc -o $@ $<

writeread:  writeread.c
	gcc -o $@ $<

writeread2:  writeread2.c
	gcc -o $@ $<

clean:
	rm -f read write serialink writeread writeread2
