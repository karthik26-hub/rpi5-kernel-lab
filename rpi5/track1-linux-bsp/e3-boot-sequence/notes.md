# Notes — E3: Boot Sequence Logger

## What This Task Teaches
- `ktime_get_boottime()` — monotonic clock from boot, survives suspend
- `ktime_to_ms()` — convert ktime_t to milliseconds
- `kobject_create_and_add()` — create /sys/kernel/<name>/ directory
- `sysfs_create_group()` — register multiple attributes at once
- `__ATTR_RO()` — read-only sysfs attribute macro
- `sysfs_emit()` — correct way to write sysfs show buffers (replaces sprintf)
- `saved_command_line` — kernel cmdline available to modules

## Errors Hit

### Minor: cmdline sysfs attribute → No such file or directory
```
cat: /sys/kernel/boot_logger/cmdline: No such file or directory
```
`saved_command_line` compiled in but sysfs attribute didn't register correctly.
Not a blocker — `/proc/cmdline` gives identical data.
```bash
cat /proc/cmdline
```

## What Worked
```
[ 6422.990163] boot_logger: === loaded at +6422750 ms since boot ===
[ 6422.990172] boot_logger: kernel  : 6.12.75+rpt-rpi-2712
[ 6422.990174] boot_logger: arch    : aarch64
[ 6441.030262] boot_logger: unloaded at +6440790 ms (resident 18040 ms)
```

sysfs attributes working:
```
/sys/kernel/boot_logger/info      → kernel, hostname, arch, boot_ms
/sys/kernel/boot_logger/uptime_ms → live ktime_get_boottime() on every read
```

## Key Observations

**boot_ms = 6422750** — module loaded ~107 minutes after boot.
Shows clearly where userspace/manual module loading sits in the boot timeline
relative to kernel init (t=0).

**uptime_ms increments live** — every `cat /sys/kernel/boot_logger/uptime_ms`
calls the `show` function which calls `ktime_get_boottime()` fresh.
This is how sysfs is meant to work — not cached values.

**RPi5 cmdline contains useful info:**
```
reboot=w coherent_pool=1M
cgroup_disable=memory          ← Raspbian default, disables memory cgroups
cfg80211.ieee80211_regdom=IN   ← India WiFi regulatory domain
console=ttyAMA10,115200        ← RPi5 UART (RP1 UART, not PL011)
smsc95xx.macaddr=...           ← USB ethernet MAC set at boot
vc_mem.mem_base/size           ← VideoCore GPU memory split
```

**resident time tracking** — `unload_ms - boot_ms` gives exact time the
module was loaded. Useful pattern for any driver that needs to track
how long it's been active.

## sysfs Pattern (reusable template)
```c
// 1. define show function
static ssize_t foo_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sysfs_emit(buf, "value: %d\n", my_value);
}

// 2. declare attribute
static struct kobj_attribute attr_foo = __ATTR_RO(foo);

// 3. group
static struct attribute *attrs[] = { &attr_foo.attr, NULL };
static struct attribute_group grp = { .attrs = attrs };

// 4. create in init
kobj = kobject_create_and_add("mydriver", kernel_kobj);
sysfs_create_group(kobj, &grp);

// 5. destroy in exit
sysfs_remove_group(kobj, &grp);
kobject_put(kobj);
```
This pattern is used in nearly every real kernel driver. M1 onwards will use it heavily.

## Commands That Matter
```bash
cat /sys/kernel/boot_logger/info        # static info snapshot
cat /sys/kernel/boot_logger/uptime_ms   # live uptime
cat /proc/cmdline                        # full kernel cmdline
dmesg | grep boot_logger                # module log
```
