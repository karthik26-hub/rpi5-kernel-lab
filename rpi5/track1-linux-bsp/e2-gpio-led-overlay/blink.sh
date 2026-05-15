#!/bin/bash
# blink.sh - RPi5 Track 1, E2: Blink ACT LED via sysfs
#
# Usage:
#   sudo ./blink.sh [count] [delay_ms]
#   sudo ./blink.sh 10 200     # blink 10 times, 200ms interval
#
# Requires: root (sysfs LED control needs root)

LED="/sys/class/leds/ACT"
COUNT=${1:-10}        # default 10 blinks
DELAY=${2:-500}       # default 500ms

if [ ! -d "$LED" ]; then
    echo "ERROR: $LED not found. Is this an RPi5?"
    exit 1
fi

# Save original trigger so we can restore it
ORIG_TRIGGER=$(cat "$LED/trigger" | grep -oP '(?<=\[)[^\]]+')
echo "Saved original trigger: $ORIG_TRIGGER"

# Take manual control of the LED
echo "none" > "$LED/trigger"
echo "0"    > "$LED/brightness"

echo "Blinking ACT LED $COUNT times (${DELAY}ms interval)..."

for i in $(seq 1 $COUNT); do
    echo "1" > "$LED/brightness"
    sleep "$(echo "scale=3; $DELAY/1000" | bc)"
    echo "0" > "$LED/brightness"
    sleep "$(echo "scale=3; $DELAY/1000" | bc)"
    echo "  Blink $i/$COUNT"
done

# Restore original trigger
echo "$ORIG_TRIGGER" > "$LED/trigger"
echo "Done. Restored trigger: $ORIG_TRIGGER"
