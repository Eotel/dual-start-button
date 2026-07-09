# Browser flashing and M5StickC Plus validation

## Goal

Flash and validate two USB-connected M5StickC Plus devices, then add the first browser-based update path for firmware installation.

## Constraints and Non-Goals

- Keep firmware roles generic; both devices use the same `m5stick_c` firmware.
- Do not add BLE OTA in this slice. Document the next design decisions instead.
- Keep the flasher static and vanilla, consistent with the existing `web-debug` app.
- Do not commit generated binaries; generated web flasher artifacts belong in `dist/`.

## Plan of Work

1. Build `m5stick_c`, identify the generated ESP32 flashing artifacts, and upload the same firmware to both connected serial ports.
2. Add a small static `web-flasher/` page using ESP Web Tools.
3. Add a deterministic helper script that prepares an ESP Web Tools manifest and merged ESP32 binary from a PlatformIO build.
4. Extend release automation to publish app images plus browser-flasher artifacts.
5. Update README / firmware docs with USB browser flashing guidance and BLE OTA next steps.
6. Verify host tests, firmware native tests, `m5stick_c` build, manifest generation, and as much real-device/browser behavior as available.

## Progress

- [done] Confirmed repo is on `main`, clean at start.
- [done] Confirmed two USB serial ports are present: `/dev/cu.usbserial-5152E15E8D` and `/dev/cu.usbserial-5D521A0D8B`.
- [done] Confirmed `pio` is available via `devenv shell -- pio`.
- [done] Built `m5stick_c` successfully.
- [done] Generated `dist/web-flasher/m5stick_c/manifest.json` and merged binary.
- [done] Uploaded to both M5StickC Plus devices.
- [done] Confirmed serial heartbeat on both devices.
- [done] Implemented web flasher files and artifact-prep script.
- [done] Updated docs and release workflow.
- [done] Ran host / firmware / hook verification.
- [done] Fixed M5StickC Plus display flicker / sluggish button feedback by replacing periodic full-screen redraws with state-change-only partial updates.
- [done] Re-flashed both USB-connected M5StickC Plus devices with the display update fix.
- [done] Changed firmware BLE local name from `DSB-<last4 device_id>` to `DSB-<6 hex chars derived from device_hash>` to avoid collisions on M5StickC Plus devices whose eFuse-derived IDs share the same trailing bytes.
- [done] Re-flashed both USB-connected M5StickC Plus devices with the collision-resistant local-name firmware.
- [done] Updated `web-debug` to show connected count, slot link status, and `DeviceInfo.name + short device_id` so duplicate or stale chooser names are still distinguishable after connect.
- [done] Fixed `web-debug` DOM churn that closed the Raw accordion and made device action buttons feel sluggish; device cards are now reused, Raw open state is preserved, and device action buttons disable immediately while a command is in flight.
- [done] Stabilized slot panel height and split slot text into fixed rows to avoid layout jumps during freshness updates.
- [done] User reported that the Chrome Web Bluetooth connect/link flow worked after the latest firmware and `web-debug` updates.

## Verification

- Passed: `devenv shell -- pio run -e m5stick_c -d firmware/pio`
- Passed: `devenv shell -- python3 tools/prepare_web_flasher.py --env m5stick_c --tag local`
- Passed: `devenv shell -- pytest tools/` (74 passed)
- Passed: `cd web-debug && node --test && node --check app.js` (89 node tests passed)
- Passed: `node --check web-flasher/app.js`
- Passed: `devenv test`
- Passed: upload to `/dev/cu.usbserial-5152E15E8D`; esptool detected ESP32-PICO-D4, MAC `d4:d4:da:bc:fe:c0`, and verified hashes for all written regions.
- Passed: upload to `/dev/cu.usbserial-5D521A0D8B`; esptool detected ESP32-PICO-D4, MAC `d4:d4:da:84:af:38`, and verified hashes for all written regions.
- Passed: serial monitor on both ports showed heartbeat packets (`state type=2 flags=0x02 slot=0 group=0`).
- Passed after display fix: both M5StickC Plus devices were re-flashed successfully and both serial monitors again showed heartbeat packets.
- Passed after display fix: `devenv shell -- pio run -e m5atom_lite -e m5stick_c -e m5stack_atoms3 -e generic_esp32_gpio -d firmware/pio` (4 succeeded).
- Passed after display fix: `devenv test`.
- Passed after local-name fix: `devenv shell -- pio run -e m5stick_c -d firmware/pio`.
- Passed after local-name fix: upload to `/dev/cu.usbserial-5152E15E8D`; esptool detected ESP32-PICO-D4, MAC `d4:d4:da:bc:fe:c0`, and verified hashes for all written regions.
- Passed after local-name fix: upload to `/dev/cu.usbserial-5D521A0D8B`; esptool detected ESP32-PICO-D4, MAC `d4:d4:da:84:af:38`, and verified hashes for all written regions.
- Passed after local-name fix: serial monitor on both ports showed heartbeat packets (`state type=2 flags=0x02 slot=0 group=0`).
- Passed after web-debug responsiveness fix: `cd web-debug && node --test && node --check app.js` (92 node tests passed).
- Passed after web-debug responsiveness fix: `npm exec --yes --package=playwright -- playwright screenshot --viewport-size=1280,900 http://localhost:8081/web-debug/ /tmp/dsb-web-debug-after-layout.png`.
- Passed after web-debug responsiveness fix: `devenv test`.
- Passed: `curl` served `web-flasher` HTML and `manifest.json`.
- Passed: Chrome/Playwright screenshots for `web-flasher` and `web-debug` were captured under `dist/`.
- User-reported pass: Chrome Web Bluetooth two-device connect/link flow after the latest firmware and `web-debug` updates.
- Not directly agent-verified: full physical start-condition path with button presses, because it requires looking at the device display, browser device-picker selection, and physical button presses.

## Decision Log

- Use ESP Web Tools for the first browser flashing path. Its documentation recommends a merged ESP32 binary for browser flashing, so the helper will generate a merged image instead of exposing raw PlatformIO parts directly.
- Keep BLE OTA out of this slice because it needs partition/rollback/protocol decisions and would make first hardware validation harder to isolate.

## Surprises and Discoveries

- `pio` is not on the default shell PATH but is installed by the repo's `devenv` environment.
- PlatformIO's build emits `bootloader.bin`, `partitions.bin`, and `firmware.bin`; `boot_app0.bin` comes from the `framework-arduinoespressif32` package.
- `npm exec --package=playwright -- playwright screenshot --channel chrome ...` works for browser screenshots without adding Playwright to the repo.
- M5StickC Plus display flicker came from `updateDisplay()` calling `showText()` every 500ms, and `showText()` clearing the entire screen. The fix is state-change-only display refresh plus line-level redraw.
- The original `DSB-<last4 device_id>` local name can collide on the two current M5StickC Plus devices; both IDs ended with `DAD4D4`, so Chrome showed both as `DSB-D4D4` before the hash-suffix fix.
- Chrome's Web Bluetooth chooser can keep showing an older paired name even after firmware changes. The host must treat `DeviceInfo.device_id` as identity and the debug UI now shows that identity after connect.
- `web-debug` rebuilt all device cards every 250ms. That made `<details>` Raw collapse and could replace action buttons during a click. Reusing card nodes fixes both symptoms.

## Outcomes and Retrospective

- Added `web-flasher/` as a static ESP Web Tools page.
- Added `tools/prepare_web_flasher.py` and tests for manifest generation / asset copying.
- Release workflow now packages `dual-start-button-web-flasher-m5stick_c-<tag>.zip`.
- README and firmware docs now document M5StickC Plus USB upload, browser flashing, and BLE OTA follow-up decisions.
- Firmware now avoids idle full-screen redraws, reducing display flicker and avoiding display SPI work in the normal button polling loop.
- The remaining direct-agent gap is physical start-condition validation with button presses; the user confirmed the Chrome connect/link workflow works.
