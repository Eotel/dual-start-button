# HID keyboard fallback

HID fallback is a constrained iPhone browser path. It is not the canonical Dual Start Button protocol.

Use normal BLE GATT firmware for:

- Desktop Chrome / Edge Web Bluetooth debugging.
- Android Chrome Web Bluetooth.
- Native iOS apps that can use CoreBluetooth.
- Any flow that needs DeviceInfo, heartbeat, link/unlink, identify, arm/disarm, or future OTA.

Use HID fallback only when an iPhone browser surface must receive button input and Web Bluetooth is unavailable.

## Firmware mode

Build and flash the M5StickC / M5StickC Plus HID fallback image:

```bash
devenv shell -- pio run -e m5stick_c_hid -d firmware/pio
devenv shell -- pio run -e m5stick_c_hid -d firmware/pio -t upload --upload-port /dev/cu.usbserial-XXXXXXXX
```

The HID build advertises as a Bluetooth keyboard named `DSB-HID-<suffix>`.
It maps the stable ESP32-derived device hash to one of the F13-F24 keyboard usages.
This keeps firmware generic: the device does not know slot 1 or slot 2.

## Return to normal GATT firmware

Flash the normal image over USB:

```bash
devenv shell -- pio run -e m5stick_c -d firmware/pio
devenv shell -- pio run -e m5stick_c -d firmware/pio -t upload --upload-port /dev/cu.usbserial-XXXXXXXX
```

For non-developer recovery, build and serve the browser flasher bundle for the target mode:

```bash
devenv shell -- python3 tools/prepare_web_flasher.py --env m5stick_c --tag local
devenv shell -- python3 tools/prepare_web_flasher.py --env m5stick_c_hid --tag local
```

Normal GATT and HID fallback are separate firmware images. There is no BLE OTA path in HID mode.

## Web-debug flow

Open `web-debug` and use the separate `HID Keyboard Fallback` section.

1. Pair each HID device with the OS as a Bluetooth keyboard.
2. Click `Capture Slot 1`, press the first physical button, then release it.
3. Click `Capture Slot 2`, press the second physical button, then release it.
4. Press each assigned button again to toggle each HID slot active.
5. Confirm the HID fallback start condition becomes ready only when both assigned slots are active and the page is focused.

The capture press seeds the baseline and does not toggle active. This avoids starting from the assignment action itself.

## Limitations

- No browser-visible GATT `device_id`.
- No heartbeat or stale-state freshness.
- No reliable disconnect event.
- No Control characteristic, link/unlink, identify, arm/disarm, or device-side ownership.
- No OTA.
- Assignments are local browser bindings and may need to be captured again on another browser/device.
- If iOS or the browser reports two devices as the same key identity, the web app blocks two-slot assignment.

## Validation log

Record every real iPhone acceptance run here.

### 2026-07-09 local implementation status

- Firmware build: `m5stick_c_hid`
- Key mapping policy: device hash modulo F13-F24
- Web app behavior covered by host tests: capture, persistence, duplicate rejection, clearing, reassignment, down-edge toggle, repeat suppression, keyup release, missed-keyup timeout, focus-gated start condition
- iPhone two-device acceptance: not yet verified in this repository by Codex; requires a physical iPhone browser run with two paired devices

Use this template for the physical run:

```text
Date:
iPhone model:
iOS version:
Browser surface: Safari tab / Home Screen web app
Device models:
Firmware env/tag:
Pairing steps:
Observed slot 1 key identity:
Observed slot 2 key identity:
Duplicate/ambiguous? yes/no
Slot assignment result:
Active toggle result:
Start condition result:
Keyup reliability:
Key repeat behavior:
Focus/blur behavior:
Missed-keyup timeout behavior:
Recommendation: continue HID fallback / stop and prioritize native iOS
Evidence:
```
