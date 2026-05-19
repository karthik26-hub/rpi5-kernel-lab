# RPi5 Kernel 6.12 — Error Reference
> Running log of every real error hit during Track 1 development.
> Updated after each assignment. Use this before starting new modules.

---

## Kernel 6.12 Strict Include Policy

Kernel 6.12 no longer pulls in transitive headers. Everything must be explicitly included.
**Rule: if you use a function, include its header. Don't assume it comes in via linux/module.h.**

| Symbol | Header required |
|--------|----------------|
| `utsname()` | `#include <linux/utsname.h>` |
| `msleep()` | `#include <linux/delay.h>` |
| `ktime_get_boottime()` | `#include <linux/timekeeping.h>` |
| `kobject_create_and_add()` | `#include <linux/kobject.h>` |
| `sysfs_emit()` | `#include <linux/sysfs.h>` |
| `gpio_device_find_by_label()` | `#include <linux/gpio/driver.h>` |
| `gpiod_set_value()` | `#include <linux/gpio/consumer.h>` |

---

## RPi5 GPIO Architecture (Critical — Read Before Any GPIO Work)

### The Problem
RPi5 moved from BCM2835 GPIO (used on RPi4) to the **RP1 south bridge chip**.

- On RPi4: `gpio_to_desc(17)` works — global GPIO 17 = header pin GPIO17
- On RPi5: `gpio_to_desc(17)` returns NULL — global GPIO 17 belongs to `gpio-brcmstb`, not the header

### RPi5 GPIO Chip Map
```
gpiochip0  →  pinctrl-rp1   base=569  ngpio=54  ← THIS is the 40-pin header
gpiochip1  →  gpio-brcmstb  base=512  ngpio=15
gpiochip2  →  gpio-brcmstb  base=527  ngpio=6
...
```

Run this before any GPIO work on RPi5:
```bash
gpioinfo | head -40        # see chip labels and line names
cat /sys/class/gpio/gpiochip*/label
cat /sys/class/gpio/gpiochip*/base
```

### The Fix (kernel 6.12 API)
```c
// OLD (RPi4 / kernel <6.12) — BROKEN on RPi5
led_gpio = gpio_to_desc(17);  // returns NULL on RPi5

// NEW (RPi5 / kernel 6.12)
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>

struct gpio_device *gdev = gpio_device_find_by_label("pinctrl-rp1");
struct gpio_desc   *desc = gpio_device_get_desc(gdev, 17);
gpiod_direction_output(desc, 0);
// No gpiod_request() needed — removed in 6.12
```

---

## Removed APIs in Kernel 6.12

These functions existed in older kernels and are commonly shown in tutorials.
**They do not exist in kernel 6.12. Do not use them.**

| Removed function | Replacement |
|-----------------|-------------|
| `gpiochip_find()` | `gpio_device_find_by_label()` or `gpio_device_find()` |
| `gpiochip_get_desc()` | `gpio_device_get_desc()` |
| `gpiod_request()` | Not needed with `gpio_device_get_desc()` |
| `gpiod_free()` | `gpio_device_put()` on the `gpio_device` |
| `led_trigger_event(classdev, ...)` | Takes `led_trigger*`, not `led_classdev*` |
| `led_find_trigger()` | Not exported in 6.12 — use sysfs trigger instead |

---

## Error Log by Assignment

### E1 — Hello World Kernel Module

**Error:** `implicit declaration of function 'utsname'`
```
error: implicit declaration of function 'utsname'; did you mean 'putname'?
```
**Cause:** `utsname()` declared in `<linux/utsname.h>`, not pulled in transitively by `<linux/module.h>` in kernel 6.12.
**Fix:** Add `#include <linux/utsname.h>`
**Lesson:** Kernel 6.12 requires explicit includes. When you get "implicit declaration", check which header owns that symbol.

---

### E2 — GPIO17 External LED Driver

**Error 1:** `insmod: ERROR: could not insert module: No such device`
```
[ x.x] gpio17_led: gpio_to_desc(17) failed
```
**Cause:** RPi5 uses RP1 south bridge. `gpio_to_desc(17)` targets global GPIO 17 which is on `gpio-brcmstb`, not the header GPIO controller.
**Diagnosis:** `gpioinfo` showed `gpiochip0 = pinctrl-rp1` with 54 lines. GPIO17 = line 17 on that chip.
**Fix:** Use `gpio_device_find_by_label("pinctrl-rp1")` + `gpio_device_get_desc(gdev, 17)`

**Error 2:** `implicit declaration of function 'gpiochip_find'`
```
error: implicit declaration of function 'gpiochip_find'; did you mean 'gpio_device_find'?
```
**Cause:** `gpiochip_find()` removed in kernel 6.12. Compiler hint pointed to replacement.
**Fix:** `gpio_device_find_by_label()` — available in `<linux/gpio/driver.h>`

**Error 3:** `implicit declaration of function 'gpiod_request'`
```
error: implicit declaration of function 'gpiod_request'
error: implicit declaration of function 'gpiod_free'
```
**Cause:** Both removed in kernel 6.12 GPIO API rework.
**Fix:** Not needed — `gpio_device_get_desc()` in 6.12 doesn't require explicit request/free cycle. Just `gpio_device_put(gdev)` on cleanup.

**Error 4:** `implicit declaration of function 'msleep'`
```
error: implicit declaration of function 'msleep'
```
**Cause:** `msleep()` in `<linux/delay.h>` — not pulled in transitively.
**Fix:** Add `#include <linux/delay.h>`

**Error 5:** `led_trigger_event` wrong argument type
```
error: passing argument 1 of 'led_trigger_event' from incompatible pointer type
note: expected 'struct led_trigger *' but argument is of type 'struct led_classdev *'
```
**Cause:** `led_trigger_event()` signature changed — takes `led_trigger*` not `led_classdev*`.
`led_find_trigger()` also not exported in 6.12.
**Fix:** Dropped runtime trigger API entirely. Used raw `gpiod_set_value()` + `msleep()` loop for blink-on-load. Triggers still work via sysfs after load.

---

## Diagnostics Cheatsheet (RPi5)

```bash
# GPIO
gpioinfo                              # all chips, all lines
cat /sys/class/gpio/gpiochip*/label   # chip labels
cat /sys/class/gpio/gpiochip*/base    # chip base numbers

# Kernel symbols — check if a function exists in running kernel
sudo cat /proc/kallsyms | grep gpio_device_find_by_label

# Headers — find which header declares a function
grep -r "gpio_device_find_by_label" /usr/src/linux-headers-$(uname -r)-common-rpi/include/

# Module errors
dmesg | tail -20
sudo modinfo <module>.ko

# LED sysfs
ls /sys/class/leds/
cat /sys/class/leds/<name>/trigger
echo heartbeat | sudo tee /sys/class/leds/<name>/trigger
```

---

*Last updated: E3 complete*

### M3 — systemd Module Autoload

**Error: boot_logger disagrees about version of symbol module_layout**
Kernel auto-updated from 6.12.75 to 6.18.29 between E3 and M3.
Old .ko built against 6.12 — vermagic mismatch, modprobe refuses to load.
Fix: rebuild all modules after any kernel update.
Rule: always check `uname -r` matches `modinfo <mod> | grep vermagic` before installing.

**Lesson: kernel updates break all out-of-tree modules**
Production systems pin kernel versions or use DKMS to auto-rebuild modules.
DKMS (Dynamic Kernel Module Support) is the proper solution for this.
