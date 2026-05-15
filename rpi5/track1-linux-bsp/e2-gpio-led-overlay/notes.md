# Notes — E2: GPIO17 External LED Driver

## What This Task Teaches
- LED class driver: led_classdev, brightness_set callback
- sysfs LED interface: /sys/class/leds/<name>/brightness, trigger
- GPIO descriptor API (kernel 6.12)
- RPi5 RP1 GPIO architecture vs RPi4 BCM2835
- Kernel module cleanup: unregister before free

## Hardware
- GPIO 17 (Pin 11) → 1kΩ resistor → LED anode → LED cathode → Pin 6 (GND)
- 3.3V − ~2V (LED forward voltage) = 1.3V across 1kΩ = 1.3mA — safe for RPi5 GPIO

## Errors Hit (in order)

### Attempt 1 — gpio_to_desc(17) → No such device
Used `gpio_to_desc(17)` assuming global GPIO number = header pin number like RPi4.
```
[ x.x] gpio17_led: gpio_to_desc(17) failed
insmod: ERROR: could not insert module: No such device
```
**Root cause:** RPi5 uses RP1 south bridge. `gpio_to_desc(17)` finds global GPIO 17
which lives on `gpio-brcmstb` (internal chip), not the 40-pin header controller.

Diagnosed with:
```bash
gpioinfo | head -40
cat /sys/class/gpio/gpiochip*/label
cat /sys/class/gpio/gpiochip*/base
```
Output showed:
```
gpiochip0 - pinctrl-rp1 - 54 lines  ← this is the header
base = 569
```
So real global number for header GPIO17 = 569 + 17 = 586. But `gpio_to_desc(586)`
is not the right approach either — use the new API instead.

### Attempt 2 — gpiochip_find() → implicit declaration
```
error: implicit declaration of function 'gpiochip_find'; did you mean 'gpio_device_find'?
```
`gpiochip_find()` removed in kernel 6.12. Compiler itself suggested the replacement.
Also hit: `gpiochip_get_desc()`, `gpiod_request()`, `gpiod_free()` — all removed.

### Attempt 3 — led_trigger API type mismatch
```
error: passing argument 1 of 'led_trigger_event' from incompatible pointer type
note: expected 'struct led_trigger *' but argument is 'struct led_classdev *'
error: implicit declaration of function 'led_find_trigger'
```
`led_trigger_event()` signature changed in 6.12 — takes `led_trigger*` not `led_classdev*`.
`led_find_trigger()` not exported at all.
**Fix:** Dropped runtime trigger API. Used `gpiod_set_value()` + `msleep()` loop directly.
Triggers (heartbeat, timer) still fully work via sysfs after load.

### Attempt 4 — msleep() → implicit declaration
```
error: implicit declaration of function 'msleep'
```
`msleep()` in `<linux/delay.h>` — not included transitively.
Fix: `#include <linux/delay.h>`

### Final working API (kernel 6.12 + RPi5)
```c
#include <linux/gpio/driver.h>    // gpio_device_find_by_label, gpio_device_get_desc
#include <linux/gpio/consumer.h>  // gpiod_direction_output, gpiod_set_value

struct gpio_device *gdev = gpio_device_find_by_label("pinctrl-rp1");
struct gpio_desc   *desc = gpio_device_get_desc(gdev, 17);
gpiod_direction_output(desc, 0);
// cleanup: gpio_device_put(gdev)
```

## What Worked
```
[ x.x] gpio17_led: found gpio_device 'pinctrl-rp1'
[ x.x] gpio17_led: /sys/class/leds/gpio17-led ready on pinctrl-rp1 line 17
```
- `echo 1 | sudo tee /sys/class/leds/gpio17-led/brightness` → LED ON physically
- `echo heartbeat | sudo tee /sys/class/leds/gpio17-led/trigger` → LED pulsed
- `sudo insmod gpio17_led.ko blink_on_load=1` → LED blinked 3x on load

## Key Observations
- RPi5 is a fundamentally different GPIO architecture from RPi4 — never assume
  global GPIO number = header pin number without checking gpioinfo first
- Kernel 6.12 GPIO API is cleaner: find device by label, get descriptor, done
- LED triggers (heartbeat, timer, cpu) are driven entirely by the kernel —
  no userspace loop needed, zero CPU overhead
- `sysfs_emit()` is the correct replacement for `sprintf(buf, ...)` in sysfs show functions

## Commands That Matter
```bash
gpioinfo                                           # always run first on new board
ls /sys/class/leds/                                # confirm registration
cat /sys/class/leds/gpio17-led/trigger             # see available triggers
echo heartbeat | sudo tee .../trigger              # kernel-driven blink
echo timer     | sudo tee .../trigger              # configurable timer
echo 200       | sudo tee .../delay_on             # 200ms on
echo 800       | sudo tee .../delay_off            # 800ms off
dmesg | grep gpio17_led                            # full module log
```
