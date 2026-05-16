# Makefile - RPi5 Track 1, M1: Character Device Driver
#
# Targets:
#   make              - build chardev.ko + test_chardev
#   make load         - insmod
#   make unload       - rmmod
#   make test         - run full test suite
#   make shell_test   - quick shell-based test (no binary needed)
#   make stats        - show sysfs stats
#   make clean        - remove all build artifacts

obj-m += chardev.o

KDIR  := /lib/modules/$(shell uname -r)/build
DEV   := /dev/chardev
STATS := /sys/kernel/chardev/stats

all: module test_chardev

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test_chardev: test_chardev.c
	gcc -Wall -o test_chardev test_chardev.c
	@echo "Built test_chardev"

load:
	sudo insmod chardev.ko
	@echo "--- dmesg ---"
	@dmesg | tail -8
	@echo ""
	@echo "--- /dev/chardev ---"
	@ls -la $(DEV)

unload:
	sudo rmmod chardev
	@dmesg | tail -3

test:
	sudo ./test_chardev
	@echo ""
	@echo "--- kernel logs ---"
	@dmesg | tail -15

shell_test:
	@echo "--- write ---"
	@echo "Hello from shell" | sudo tee $(DEV)
	@echo ""
	@echo "--- read ---"
	@sudo cat $(DEV)
	@echo ""
	@echo "--- stats ---"
	@cat $(STATS)

stats:
	@cat $(STATS)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test_chardev
