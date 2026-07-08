# Dual Start Button

M5Stack family devicesをBLE GATT Peripheralとして動作させ、スマホ側で任意のn台から2台を選んで同時押下開始条件を作るための仕様・firmware・debug web appです。

## Contents

```text
SPEC.md                  Protocol and operation specification
firmware/pio             PlatformIO firmware project
web-debug                Simple Web Bluetooth debug app
```

## Quick start

### Firmware

```bash
cd firmware/pio
pio run -e m5atom_lite
pio run -e m5atom_lite -t upload
pio device monitor -b 115200
```

Other envs:

```bash
pio run -e m5stick_c
pio run -e m5stack_atoms3
pio run -e generic_esp32_gpio
```

### Debug app

```bash
cd web-debug
python3 -m http.server 8080
```

Open `http://localhost:8080` in a Web Bluetooth compatible browser.

## Operational summary

1. Flash the same firmware to all button devices.
2. Open the debug app or the real smartphone app.
3. Connect any number of devices.
4. Link two chosen devices to slot 1 and slot 2.
5. Start condition becomes true when both linked slots are connected, armed, fresh, and pressed simultaneously for the confirmation window.
6. To replace a device or phone, scan again and force link the desired devices.
