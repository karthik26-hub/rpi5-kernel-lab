# Notes — M2: I2C Sensor Driver (DS3231 RTC)

## Hardware confirmed working
- ZS-042 module detected on i2c-1
- 0x68 = DS3231 RTC
- 0x57 = AT24C32 EEPROM (same module)

## Raw register reads confirmed
- 0x00 = 0x37 (37 seconds, BCD)
- 0x01 = 0x45 (45 minutes, BCD)
- 0x02 = 0x02 (02 hours, BCD)
- 0x11 = 0x1a (26°C temperature)

## BCD decode formula
#define BCD2BIN(bcd)  (((bcd) >> 4) * 10 + ((bcd) & 0x0F))

## Status
Paused — will return after M3/H-level work.
Need to understand probe() lifecycle better before writing the driver.
