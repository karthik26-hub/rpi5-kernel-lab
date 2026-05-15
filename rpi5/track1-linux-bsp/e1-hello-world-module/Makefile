# Makefile - RPi5 Track 1, E1: Hello World Kernel Module
#
# Usage:
#   make          - build hello.ko against running kernel
#   make clean    - remove build artifacts

obj-m += hello.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
