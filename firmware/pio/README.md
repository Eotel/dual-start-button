# Firmware

PlatformIO project for Dual Start Button devices.

## Build

```bash
pio run -e m5atom_lite
```

## Upload

```bash
pio run -e m5atom_lite -t upload
pio device monitor -b 115200
```

## Environments

| Env | Board | Button source |
|---|---|---|
| `m5atom_lite` | `m5stack-atom` | `M5.BtnA` |
| `m5stick_c` | `m5stick-c` | `M5.BtnA` |
| `m5stack_atoms3` | `m5stack-atoms3` | `M5.BtnA` |
| `generic_esp32_gpio` | `esp32dev` | `DSB_BUTTON_GPIO` |

## Button source override

Edit `platformio.ini` build flags:

```ini
-D DSB_BUTTON_SOURCE=4
-D DSB_BUTTON_GPIO=39
-D DSB_BUTTON_ACTIVE_LEVEL=0
```

Button source constants are defined in `include/config.h`.

## Long hold reset

Holding the configured button for `DSB_LONG_HOLD_RESET_MS` clears only device-side link information. It does not erase firmware or the stable device ID.
