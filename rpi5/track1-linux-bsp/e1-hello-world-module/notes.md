# Notes — E1: Hello World Kernel Module

## What This Task Teaches
- LKM skeleton: module_init, module_exit, MODULE_LICENSE
- Module parameters: module_param(), MODULE_PARM_DESC()
- printk log levels: KERN_INFO, KERN_DEBUG
- How the kernel loads/unloads out-of-tree modules

## Errors Hit

### Error: implicit declaration of utsname()
```
error: implicit declaration of function 'utsname'; did you mean 'putname'?
```
- Tried to use `utsname()->release` to print kernel version in debug output
- Kernel 6.12 does not pull `<linux/utsname.h>` transitively via `<linux/module.h>`
- Fix: `#include <linux/utsname.h>` explicitly

**Pattern learned:** Kernel 6.12 strict include policy — any implicit declaration error
means find the header that owns that symbol and add it explicitly.

## What Worked
```
[ 1929.290069] hello: module loaded (debug_level=0)
[ 1954.011518] hello: module loaded (debug_level=1)
[ 1954.011525] hello: [DEBUG] running on kernel 6.12.75+rpt-rpi-2712
[ 1954.011527] hello: [DEBUG] module load complete
```

## Key Observations
- "loading out-of-tree module taints kernel" — expected for any module not in mainline tree
- `modinfo` shows all MODULE_* macros cleanly: author, description, version, parm
- `vermagic: 6.12.75+rpt-rpi-2712 SMP preempt mod_unload modversions aarch64`
  confirms module is tied to exact kernel version — won't load on a different kernel build

## Commands That Matter
```bash
sudo insmod hello.ko debug_level=1   # load with param
lsmod | grep hello                   # confirm loaded
modinfo hello.ko                     # inspect metadata
dmesg | tail -10                     # see printk output
sudo rmmod hello                     # unload
```
