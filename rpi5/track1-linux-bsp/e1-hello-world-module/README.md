# [RPi5] Track 1 — E1: Hello World Kernel Module

## Task
Write a Linux kernel module using `module_init`/`module_exit` with a `debug_level` parameter.
Verify with `lsmod`, `modinfo`, `dmesg`.

## Learning Outcomes
- LKM skeleton: `module_init`, `module_exit`, `MODULE_LICENSE`, `MODULE_PARM_DESC`
- Module parameters: `module_param()` and reading them at load time
- `printk` log levels: `KERN_INFO`, `KERN_DEBUG`
- Module loading/unloading lifecycle

## Prerequisites
```bash
sudo apt install linux-headers-$(uname -r) build-essential
```

## Build
```bash
make
```
Expected output: `hello.ko` built successfully.

## Load & Test
```bash
# Basic load
sudo insmod hello.ko
dmesg | tail -5

# With debug parameter
sudo rmmod hello
sudo insmod hello.ko debug_level=1
dmesg | tail -10

# Inspect the loaded module
lsmod | grep hello
modinfo hello.ko

# Unload
sudo rmmod hello
dmesg | tail -3
```

## Expected dmesg Output
```
[ 1234.567] hello: module loaded (debug_level=0)
[ 1234.568] hello: [DEBUG] running on kernel 6.x.x-rpi-v8+   ← only if debug_level=1
[ 1235.890] hello: module unloaded
```

## Auto-load on Boot (optional)
```bash
sudo cp hello.ko /lib/modules/$(uname -r)/kernel/drivers/misc/
sudo depmod -a
echo "hello" | sudo tee -a /etc/modules
```

## Files
| File | Purpose |
|------|---------|
| `hello.c` | Kernel module source |
| `Makefile` | Build system |

## Notes
<!-- Add your debugging notes here after testing -->
## Actual Output (RPi5, kernel 6.12.75+rpt-rpi-2712)

[ 1954.011525] hello: [DEBUG] running on kernel 6.12.75+rpt-rpi-2712

### dmesg — basic load

[ 1929.289822] hello: loading out-of-tree module taints kernel.
[ 1929.290069] hello: module loaded (debug_level=0)
[ 1953.977006] hello: module unloaded
[ 1954.011518] hello: module loaded (debug_level=1)
[ 1954.011525] hello: [DEBUG] running on kernel 6.12.75+rpt-rpi-2712
[ 1954.011527] hello: [DEBUG] module load complete
[ 1970.248976] hello: module unloaded

filename:    hello.ko
version:     1.0
description: RPi5 Track1 E1 - Hello World LKM
author:      Karthik Nambiar
license:     GPL
vermagic:    6.12.75+rpt-rpi-2712 SMP preempt mod_unload modversions aarch64
parm:        debug_level:Debug verbosity level (0=off, 1=verbose) (int)

### Notes
- utsname() requires explicit #include <linux/utsname.h> on kernel 6.12 — not pulled in transitively
- "taints kernel" warning is expected for out-of-tree modules, not an error
