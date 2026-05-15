# [RPi5] Track 1 — E2: GPIO LED via sysfs + udev

## Task
Control the onboard ACT LED via `/sys/class/leds/ACT/`, write a blink shell script,
and add a udev rule to trigger it on USB plug events.

## Learning Outcomes
- sysfs LED class interface (`/sys/class/leds/`)
- LED triggers: `none`, `mmc0`, `heartbeat`, `cpu`
- udev rules: `ACTION`, `SUBSYSTEM`, `RUN`
- Overlay concepts (ACT LED is already a `leds-gpio` device in RPi5 firmware DTS)

## RPi5 ACT LED Facts
| Property | Value |
|----------|-------|
| sysfs path | `/sys/class/leds/ACT` |
| Default trigger | `mmc0` (blinks on SD card activity) |
| GPIO | GPIO 9 (internal, managed by firmware) |
| Available triggers | `none heartbeat mmc0 cpu timer` |

## Explore sysfs First
```bash
cat /sys/class/leds/ACT/trigger
echo "heartbeat"  | sudo tee /sys/class/leds/ACT/trigger
echo "cpu"        | sudo tee /sys/class/leds/ACT/trigger
echo "none"       | sudo tee /sys/class/leds/ACT/trigger
echo "1" | sudo tee /sys/class/leds/ACT/brightness
echo "0" | sudo tee /sys/class/leds/ACT/brightness
echo "mmc0" | sudo tee /sys/class/leds/ACT/trigger
```

## Blink Script
```bash
chmod +x blink.sh
sudo ./blink.sh          # 10 blinks, 500ms
sudo ./blink.sh 20 200   # 20 blinks, 200ms
```

## udev Rule
```bash
sudo cp 99-act-led.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
# Plug any USB device — ACT LED flashes once
```

## Files
| File | Purpose |
|------|---------|
| `blink.sh` | Blink ACT LED N times |
| `99-act-led.rules` | udev rule — flash on USB plug |

## Notes
<!-- Add your observations here after testing -->
