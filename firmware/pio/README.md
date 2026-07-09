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

M5StickC Plus:

```bash
devenv shell -- pio run -e m5stick_c -d firmware/pio
devenv shell -- pio run -e m5stick_c -d firmware/pio -t upload --upload-port /dev/cu.usbserial-XXXXXXXX
devenv shell -- pio device monitor -d firmware/pio -b 115200 --port /dev/cu.usbserial-XXXXXXXX
```

When two devices are connected, pass `--upload-port` explicitly for each device.

## Environments

| Env | Board | Button source |
|---|---|---|
| `m5atom_lite` | `m5stack-atom` | `M5.BtnA` |
| `m5stick_c` | `m5stick-c` | `M5.BtnA` |
| `m5stack_atoms3` | `m5stack-atoms3` | `M5.BtnA` |
| `generic_esp32_gpio` | `esp32dev` | `DSB_BUTTON_GPIO` |

## Browser USB flasher

The first non-developer update path is a static ESP Web Tools page. It packages
the PlatformIO build into a merged ESP32 binary plus `manifest.json`.

```bash
devenv shell -- pio run -e m5stick_c -d firmware/pio
devenv shell -- python3 tools/prepare_web_flasher.py --env m5stick_c --tag local
python3 -m http.server 18080 -d dist/web-flasher/m5stick_c
```

Open `http://localhost:18080` in Chrome or Edge. For hosted distribution, serve
the generated directory over HTTPS.

BLE OTA is intentionally not implemented yet. Add it only after deciding the
partition table, rollback behavior, integrity checks, and how the normal BLE
button service behaves during an update.

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
