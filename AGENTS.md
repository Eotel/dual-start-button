# AGENTS.md

Guidance for coding agents and human contributors working on this repository.

## Project summary

This repository defines and implements **Dual Start Button**, a BLE GATT based button-device system for M5Stack-family ESP32 devices.

The core product behavior is:

```text
Any number of supported M5Stack-family devices can be discovered by the host.
The host chooses two devices and links them to logical slot 1 and slot 2.
Each press of a linked button toggles that slot's host-derived active state.
The application start condition becomes true only when both linked slots are connected, armed, fresh, and active. Simultaneous pressing is not required.
```

Do not reintroduce fixed A/B device roles. Devices are generic peripherals with stable `device_id`; slot assignment is a host-level link operation.

## Repository layout

```text
SPEC.md                  Protocol, behavior, and operation specification
README.md                User-facing quick start
AGENTS.md                Contributor / agent guidance
firmware/pio             PlatformIO firmware project
firmware/pio/include     Firmware configuration and protocol headers
firmware/pio/src         Firmware implementation
web-debug                Simple vanilla Web Bluetooth debug app
tools/decode_state.py    ButtonState packet decoder
```

## Source of truth

`SPEC.md` is the product and protocol source of truth.

When changing behavior or protocol, update all affected files in the same change:

```text
SPEC.md
firmware/pio/include/protocol.h
firmware/pio/src/main.cpp
web-debug/app.js
tools/decode_state.py
README.md or component README files when commands or behavior change
```

A protocol change is incomplete unless firmware encoding, host decoding, and debug tooling still agree.

## Non-negotiable design constraints

### 1. Device role must remain generic

Do not add firmware builds such as `device_a` / `device_b` as the primary pairing model.

Correct model:

```text
Device: stable device_id + current link_group_id + current link_slot
Host: chooses two devices and assigns slot 1 / slot 2
```

### 2. Link must be replaceable

The system must support:

```text
link
unlink
force link
force unlink
reconnect
phone replacement
single-device replacement after failure
```

Do not make BLE bonding or phone-local identity the only way to recover a pair.

### 3. Start condition belongs to the host

Firmware reports state. The host decides whether both buttons satisfy the start condition.

Firmware must not assume that a single device can decide application start.

### 4. State synchronization, not press-only events

The protocol reports current state and repeats it via heartbeat.

Do not regress to a press-only event stream where release, stale state, disconnect, or notification loss cannot be recovered.

### 5. Hardware access must stay abstracted

M5Stack-family differences should be isolated behind `DeviceAdapter` and build flags.

Avoid direct hardware-specific button, LED, or display access in protocol logic. Prefer:

```cpp
device.readButtonPressed();
device.setStatusRgb(...);
device.blinkStatus(...);
device.showText(...);
```

## Firmware development

Firmware lives in:

```text
firmware/pio
```

Primary build commands:

```bash
cd firmware/pio
pio run -e m5atom_lite
pio run -e m5stick_c
pio run -e m5stack_atoms3
pio run -e generic_esp32_gpio
```

Upload and monitor:

```bash
pio run -e m5atom_lite -t upload
pio device monitor -b 115200
```

### Firmware dependencies

The firmware currently uses:

```text
Arduino framework
M5Unified
NimBLE-Arduino
Preferences / NVS
```

Do not add heavy dependencies unless they materially simplify the product. Keep the firmware small and inspectable.

### Button input

Button source is selected by build flags:

```cpp
DSB_BUTTON_SOURCE_A
DSB_BUTTON_SOURCE_B
DSB_BUTTON_SOURCE_C
DSB_BUTTON_SOURCE_PWR
DSB_BUTTON_SOURCE_GPIO
```

For generic GPIO builds, configure:

```cpp
DSB_BUTTON_GPIO
DSB_BUTTON_ACTIVE_LEVEL
```

### Timing rules

The intended defaults are:

```text
DSB_HEARTBEAT_MS = 1000
DSB_DEBOUNCE_MS = 30
DSB_LONG_HOLD_RESET_MS = 10000
```

Avoid long blocking `delay()` calls in the main loop. Timing should remain responsive for BLE, button debounce, heartbeat, and LED/display updates.

### Link persistence

Device-side link state is stored in NVS using `Preferences`.

Current persistent fields:

```text
link_group_id
link_slot
link_generation
```

A long-hold reset should clear only link information. It must not erase the stable device identity or require reflashing.

## BLE protocol contract

Service UUID:

```text
7b1f0000-6d4f-4f4a-9a4f-2d0c7a7a0001
```

Characteristics:

```text
DeviceInfo     Read             UTF-8 JSON
ButtonState    Read / Notify    fixed 20-byte binary
Control        Write / WriteNR  fixed 12-byte binary
ControlResult  Read / Notify    UTF-8 JSON
```

### ButtonState packet

ButtonState is fixed-size little-endian binary, exactly 20 bytes:

```cpp
struct ButtonStateV1 {
  uint8_t  version;
  uint8_t  type;
  uint8_t  flags;
  uint8_t  link_slot;
  uint16_t seq;
  uint32_t uptime_ms;
  uint32_t device_hash;
  uint32_t link_group_id;
  uint16_t aux;
};
```

Do not change the size, field order, endianness, or meaning without updating `SPEC.md`, firmware encode/decode helpers, web-debug parsing, and `tools/decode_state.py` together.

### Control packet

Control is fixed-size little-endian binary, exactly 12 bytes:

```cpp
struct ControlCommandV1 {
  uint8_t  version;
  uint8_t  command;
  uint8_t  slot;
  uint8_t  flags;
  uint32_t group_id;
  uint32_t value;
};
```

Current commands:

```text
1 LINK
2 UNLINK
3 SET_ARMED
4 IDENTIFY
5 FACTORY_RESET_LINK
```

`flags bit0` is the force flag for commands that need conflict override semantics.

### Notification policy

Firmware should notify on:

```text
button down
button up
link/unlink/factory reset
error state
heartbeat
```

Host software must read ButtonState after subscribing. Notify alone is not the source of truth.

## Host / web-debug development

The debug app lives in:

```text
web-debug
```

Run locally:

```bash
cd web-debug
python3 -m http.server 8080
```

Open:

```text
http://localhost:8080
```

The debug app is intentionally vanilla HTML/CSS/JavaScript. Do not introduce a bundler or framework unless the app becomes large enough to justify it.

### Host behavior rules

The host must:

```text
scan by service UUID
connect to any number of candidate devices
read DeviceInfo
subscribe to ButtonState and ControlResult
read ButtonState immediately after subscription
allow link/unlink/force link/force unlink
persist local pair metadata for convenience only
toggle a slot's active state on each observed press down-edge
treat the first observed state after (re)connect as a baseline, never a toggle
treat disconnect as not pressed
reset a slot's active whenever its device binding or connection changes (disconnect, unlink, relink, new group), seeding the baseline from the slot's last known pressed state when available
treat stale state as invalid for start
```

The local pair in `localStorage` is a convenience cache. It must not be treated as an exclusive ownership record.

### Start condition

The host-side start condition should follow `SPEC.md`:

```text
slot1 connected
slot2 connected
slot1 armed
slot2 armed
slot1 fresh
slot2 fresh
slot1 active
slot2 active
```

Active is host-derived: each observed press down-edge of the linked device toggles it. Simultaneous pressing is not required.

Default host constants:

```text
STALE_MS = 1500
CONTROL_RESULT_TIMEOUT_MS = 3000
```

## Debugging and validation

Decode a ButtonState packet:

```bash
python3 tools/decode_state.py "01 02 16 01 2a 00 10 27 00 00 78 56 34 12 ef cd ab 00 00 00"
```

Manual validation checklist:

```text
1. Build firmware for at least m5atom_lite.
2. Flash two or more devices with the same firmware.
3. Open web-debug over localhost.
4. Connect more than two devices if available.
5. Link any two devices to slot 1 and slot 2.
6. Verify each device reports DeviceInfo and ButtonState.
7. Press each button independently and verify pressed state changes.
8. Press each linked button once and verify its slot toggles to active; press again and verify it toggles back off.
9. Toggle both slots active and verify the start condition becomes true without simultaneous pressing.
10. Unlink one slot and verify start condition becomes false.
11. Force link a different device into the same slot and verify replacement works and that slot's active resets.
12. Disconnect a linked device and verify its slot becomes not pressed/stale and its active resets.
13. Reconnect and verify the host reads current ButtonState after subscribing.
14. Long-hold reset a device and verify only link state is cleared.
```

## Coding style

### C++ firmware

Keep code readable and explicit. Prefer small helper functions over clever abstractions.

Use:

```cpp
uint8_t / uint16_t / uint32_t
little-endian encode/decode helpers
non-blocking loop logic
clear command validation
explicit error messages in ControlResult
```

Avoid:

```text
hidden global behavior that changes protocol semantics
fixed A/B role assumptions
large dynamic allocations in hot paths
blocking delays in the main loop
protocol fields that are only documented in comments
```

### JavaScript debug app

Use browser-native APIs and explicit `DataView` parsing.

Avoid framework-specific abstractions. The debug app should remain easy to open, inspect, and modify on a local machine.

### Documentation

When behavior changes, update docs in the same change. The minimum required update for behavior/protocol changes is usually `SPEC.md`.

## Safety and operational assumptions

This project is intended for local physical interaction devices, not for high-security access control.

Initial versions intentionally do not require BLE bonding. Replacement and recovery are more important than strong device ownership.

If the deployment environment is public or adversarial, add an application-layer authentication design before relying on button events for safety-critical behavior.

## Agent skills

### Issue tracker

Issues live as GitHub issues in `Eotel/dual-start-button`; external PRs are not a triage surface. See `docs/agents/issue-tracker.md`.

### Triage labels

Default label vocabulary (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context layout: `CONTEXT.md` + `docs/adr/` at the repo root. See `docs/agents/domain.md`.

## Definition of done

A change is ready when:

```text
Firmware builds for the affected PlatformIO envs, or any unbuilt envs are explicitly called out.
Web-debug still parses and displays current protocol packets.
SPEC.md reflects the current behavior.
Link, unlink, force relink, disconnect, reconnect, and stale-state behavior are not broken.
The project still supports generic M5Stack-family devices through DeviceAdapter/config rather than target-specific protocol forks.
```
