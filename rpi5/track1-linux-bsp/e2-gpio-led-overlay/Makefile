# Makefile - RPi5 Track 1, E2: External LED driver
#
# Usage:
#   make              - build gpio17_led.ko
#   make load         - insmod the module
#   make unload       - rmmod the module
#   make blink        - load with heartbeat trigger enabled
#   make test         - quick on/off sysfs test
#   make clean        - remove build artifacts

obj-m += gpio17_led.o

KDIR  := /lib/modules/$(shell uname -r)/build
LED   := /sys/class/leds/gpio17-led

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

load:
	sudo insmod gpio17_led.ko
	@echo "Loaded. Check: ls $(LED)"

blink:
	sudo insmod gpio17_led.ko blink_on_load=1
	@echo "Loaded with heartbeat trigger"

unload:
	@echo "none" | sudo tee $(LED)/trigger 2>/dev/null || true
	sudo rmmod gpio17_led
	@echo "Unloaded"

test:
	@echo "--- LED ON ---"
	@echo 1 | sudo tee $(LED)/brightness
	@sleep 1
	@echo "--- LED OFF ---"
	@echo 0 | sudo tee $(LED)/brightness
	@sleep 0.5
	@echo "--- heartbeat trigger ---"
	@echo heartbeat | sudo tee $(LED)/trigger
	@sleep 3
	@echo "--- timer trigger (500ms on, 500ms off) ---"
	@echo timer     | sudo tee $(LED)/trigger
	@echo 500       | sudo tee $(LED)/delay_on
	@echo 500       | sudo tee $(LED)/delay_off
	@sleep 3
	@echo "--- done, restoring to none ---"
	@echo none | sudo tee $(LED)/trigger
	@echo 0    | sudo tee $(LED)/brightness

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
