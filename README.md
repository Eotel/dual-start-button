# Dual Start Button

M5Stack family devicesをBLE GATT Peripheralとして動作させ、スマホ側で任意のn台から2台を選んで同時押下開始条件を作るための仕様・firmware・debug web appです。

## Contents

```text
SPEC.md                  Protocol and operation specification
firmware/pio             PlatformIO firmware project
web-debug                Simple Web Bluetooth debug app
test-vectors             Shared protocol golden vectors (cross-implementation contract)
tools                    ButtonState decoder and its tests
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

## Testing

The protocol wire formats are pinned by shared golden vectors in `test-vectors/`:

```text
test-vectors/button_state.json      20-byte ButtonState packet cases
test-vectors/control_command.json   12-byte Control packet cases
```

Each vector maps a little-endian hex packet to its raw field values. These
files are the cross-implementation contract: the Python decoder tests consume
them now, and the firmware (C++ native) and web-debug (JavaScript) suites will
consume the same files so all three implementations agree byte-for-byte. `hex`
is lowercase without spaces; `expect` fields are raw numeric values.

Run the Python decoder tests (pytest is not required globally; `uvx` fetches it):

```bash
uvx pytest tools/
# or, if pytest is already installed:
pytest tools/
```

CI (`.github/workflows/ci.yml`) runs `pytest tools/` on every push to `main`
and every pull request.

## Operational summary

1. Flash the same firmware to all button devices.
2. Open the debug app or the real smartphone app.
3. Connect any number of devices.
4. Link two chosen devices to slot 1 and slot 2.
5. Start condition becomes true when both linked slots are connected, armed, fresh, and pressed simultaneously for the confirmation window.
6. To replace a device or phone, scan again and force link the desired devices.
