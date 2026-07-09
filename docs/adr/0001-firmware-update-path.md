# ADR 0001: firmware update path

Status: accepted

## Context

Dual Start Button needs a simpler firmware update path than asking every user to
install PlatformIO. The current firmware is an ESP32 Arduino build for
M5Stack-family devices. Initial devices may be factory blank, partially flashed,
or in need of recovery after a failed experiment.

## Decision

Use USB browser flashing as the first update path:

- Build firmware with PlatformIO.
- Generate an ESP Web Tools bundle with `tools/prepare_web_flasher.py`.
- Serve the bundle from localhost during development, or HTTPS for distribution.
- Use the same `m5stick_c` firmware for M5StickC Plus devices.

BLE OTA remains a later feature. It must not be added as an opportunistic write
characteristic on the existing control service.

## BLE OTA requirements before implementation

- Partition table with OTA slots and enough app space for M5Unified/NimBLE.
- Rollback behavior for failed boot or failed validation.
- Firmware integrity check, at minimum checksum; signature if the deployment is
  public or adversarial.
- Chunking, resume/cancel semantics, timeout behavior, and progress reporting.
- Clear policy for the normal button GATT service during update.
- Recovery route that still works when BLE OTA fails.

## Consequences

USB browser flashing handles initial install and recovery first. BLE OTA can be
added later without changing the button protocol prematurely.
