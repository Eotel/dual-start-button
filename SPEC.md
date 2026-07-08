# Dual Start Button Device SPEC

Status: draft v0.1  
Target: M5Stack family button devices, initially M5 Atom Lite + TailBAT x2  
Transport: BLE GATT Peripheral, custom service  
Host: smartphone app / debug Web Bluetooth app

## 1. Goal

複数台のM5Stack系デバイスをBLE Peripheralとして動作させる。ホストアプリは任意のn台から2台を選ぶ。各ボタンは一度押すとホスト側のactive状態がtrueになり、もう一度押すとfalseに戻る。2台のactiveが両方trueになったときにアプリ開始条件を満たす。同時押しは不要。

この仕様では、ボタンデバイスを「A用」「B用」として固定しない。各デバイスは安定した `device_id` を持つ汎用ボタンPeripheralであり、ホスト側が任意の2台を `slot 1` / `slot 2` としてlinkする。

## 2. Non-goals

- BLE HIDキーボード/ゲームパッドとしての実装は対象外。
- BLE Advertisingのみで押下を送る方式は対象外。
- BLE bonding必須のセキュアペアリングは初期版では対象外。
- TailBATの正確なバッテリー残量取得は対象外。TailBAT単体ではAtom Lite側から正確な残量を読める前提を置かない。

## 3. Terms

| Term | Meaning |
|---|---|
| Device | M5 Atom Lite, M5StickC系などの物理ボタンデバイス |
| Host | スマホアプリまたはdebug web app |
| Link | Hostが任意のDeviceをslot 1/2に割り当てる操作 |
| Unlink | Device側のlink情報を解除する操作 |
| Slot | Host上の論理スロット。`1`または`2` |
| Group | Hostが生成する32-bitのlink group ID。2台のデバイスを同じペアとして扱うための識別子 |
| Armed | Hostが開始判定に使ってよい状態 |
| Pressed | 物理ボタンが現在押されている状態 |
| Active | Host側で導出するトグル状態。pressedのdown-edgeを観測するたびに反転する。wire上には存在しない |

## 4. Design decisions

### 4.1 Device role is not fixed

デバイスは `role=A/B` をファームウェアに焼き込まない。全デバイスは同一ファームウェアで動作する。

- 各Deviceは `device_id` を持つ。
- Hostが `slot 1` / `slot 2` を決める。
- Deviceは現在linkされていれば `link_group_id` と `link_slot` を保持する。
- ただし、開始条件の最終判定はHost側で行う。

### 4.2 Link is app-level, not BLE bonding

初期版ではBLE bondingを使わない。理由は運用上の交換容易性を優先するため。

- スマホが壊れても、新しいスマホからDeviceをスキャンして再linkできる。
- 既にlink済みのDeviceでも、明示的なforce linkにより別Group/Slotへ再割り当てできる。
- 誤操作を防ぐため、通常linkは既存Groupと衝突した場合に拒否し、force flag付きのlinkのみ上書きを許可する。

### 4.3 Press eventではなくstate synchronization

Hostはイベント列だけではなく、各Deviceの現在状態を持つ。Hostは観測したpressedのdown-edge（false→true遷移）ごとに、そのDeviceをlinkしているslotのactiveを反転させる。

開始条件は次のように定義する。

```text
slot1.connected == true
slot2.connected == true
slot1.armed == true
slot2.armed == true
slot1.last_received_at <= STALE_MS
slot2.last_received_at <= STALE_MS
slot1.active == true
slot2.active == true
=> start condition satisfied
```

同時押しは不要。各ボタンは一度押すとactive=true、もう一度押すとactive=falseになる。

初期推奨値:

```text
STALE_MS = 1500
HEARTBEAT_MS = 1000
DEBOUNCE_MS = 30
```

## 5. Hardware abstraction

### 5.1 Supported hardware class

M5Stack系ESP32デバイスを対象にする。初期ターゲットは次の通り。

| Target | PIO board | Button default | Notes |
|---|---|---|---|
| Atom Lite | `m5stack-atom` | `M5.BtnA` / GPIO39 | TailBAT運用想定 |
| M5StickC / M5StickC Plus | `m5stick-c` | `M5.BtnA` | 画面あり。表示debug可能 |
| AtomS3系 | `m5stack-atoms3` | `M5.BtnA` | env追加で対応 |
| Generic ESP32 | 任意 | custom GPIO | `DSB_BUTTON_SOURCE_GPIO` を使う |

M5Unifiedを使い、ボタン取得は `M5.BtnA.isPressed()` などの共通API経由にする。GPIO直読みはfallbackまたは外付けボタン用とする。

### 5.2 Button source

Build flagでボタン入力源を選ぶ。

```c
#define DSB_BUTTON_SOURCE_A      0
#define DSB_BUTTON_SOURCE_B      1
#define DSB_BUTTON_SOURCE_C      2
#define DSB_BUTTON_SOURCE_PWR    3
#define DSB_BUTTON_SOURCE_GPIO   4
```

Default:

```c
#define DSB_BUTTON_SOURCE DSB_BUTTON_SOURCE_A
```

GPIO fallback:

```c
#define DSB_BUTTON_GPIO 39
#define DSB_BUTTON_ACTIVE_LEVEL 0
```

### 5.3 LED/display feedback

Deviceは可能であればLEDまたは画面で状態を表示する。

| State | Indication |
|---|---|
| Booting | white pulse |
| Advertising/unlinked | blue slow blink |
| Connected | blue solid |
| Linked + armed | green solid |
| Pressed | red solid |
| Identify | magenta blink |
| Link conflict/error | yellow blink |
| Factory reset / unlink | cyan blink |

Atom LiteのRGB LED、M5Stick系の画面、または何もない環境を同じ `DeviceAdapter` で吸収する。

## 6. BLE GATT profile

### 6.1 Service

```text
DualStartButtonService
UUID: 7b1f0000-6d4f-4f4a-9a4f-2d0c7a7a0001
```

### 6.2 Characteristics

| Name | UUID | Property | Format | Purpose |
|---|---|---|---|---|
| DeviceInfo | `7b1f0001-6d4f-4f4a-9a4f-2d0c7a7a0001` | Read | UTF-8 JSON | device_id, firmware_version, model, protocol |
| ButtonState | `7b1f0002-6d4f-4f4a-9a4f-2d0c7a7a0001` | Read, Notify | Binary 20 bytes | pressed/armed/link/current status |
| Control | `7b1f0003-6d4f-4f4a-9a4f-2d0c7a7a0001` | Write, Write No Response | Binary 12 bytes | link, unlink, arm, identify |
| ControlResult | `7b1f0004-6d4f-4f4a-9a4f-2d0c7a7a0001` | Read, Notify | UTF-8 JSON | command result/debug |

### 6.3 Advertising

Device advertises:

```text
local_name: DSB-<last 4 hex of device_id>
service_uuid: DualStartButtonService
```

Initial version keeps advertising data minimal for compatibility. DeviceInfo read is the source of truth.

## 7. DeviceInfo characteristic

Format: UTF-8 JSON string.

Example:

```json
{
  "v": 1,
  "device_id": "A1B2C3D4E5F6",
  "device_hash": 305419896,
  "name": "DSB-E5F6",
  "fw": "0.1.0",
  "model": "m5stack-family",
  "protocol": "dual-start-button-gatt-v1",
  "link_group_id": 12345678,
  "link_slot": 1,
  "link_generation": 4
}
```

Notes:

- `device_id` はESP32 eFuse MAC由来の12桁hexを初期値とする。
- `device_hash` は `device_id` のFNV-1a 32-bit hash。
- `link_group_id == 0` または `link_slot == 0` はunlinkedを表す。

## 8. ButtonState characteristic

Format: fixed binary, little endian, exactly 20 bytes.

```c
struct ButtonStateV1 {
  uint8_t  version;        // 1
  uint8_t  type;           // 1=state, 2=heartbeat, 3=boot, 4=link, 5=error
  uint8_t  flags;          // bit flags below
  uint8_t  link_slot;      // 0=unlinked, 1=slot1, 2=slot2
  uint16_t seq;            // increments on state/link/heartbeat
  uint32_t uptime_ms;      // millis() since boot
  uint32_t device_hash;    // FNV-1a hash of device_id
  uint32_t link_group_id;  // 0 if unlinked
  uint16_t aux;            // hold_ms, error_code, or 0
};
```

Offsets:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | version |
| 1 | 1 | type |
| 2 | 1 | flags |
| 3 | 1 | link_slot |
| 4 | 2 | seq |
| 6 | 4 | uptime_ms |
| 10 | 4 | device_hash |
| 14 | 4 | link_group_id |
| 18 | 2 | aux |

Flags:

```text
bit0 pressed
bit1 armed
bit2 linked
bit3 long_pressed
bit4 connected
bit5 error
bit6 reserved
bit7 reserved
```

Types:

```text
1 state       physical button state changed
2 heartbeat   current state repeated periodically
3 boot        boot state; also available through Read
4 link        link/unlink state changed
5 error       error state or rejected command
```

Packets with `type == 5 (error)` set flags bit5 in addition to the current state bits.

### Notification policy

Device sends ButtonState Notify:

- on connection, after service is available if subscribed later via Read
- on button down
- on button up
- on link/unlink/factory reset
- every `HEARTBEAT_MS`

Host must call Read immediately after subscribing to obtain current state.

## 9. Control characteristic

Format: fixed binary, little endian, exactly 12 bytes.

```c
struct ControlCommandV1 {
  uint8_t  version;       // 1
  uint8_t  command;       // command enum
  uint8_t  slot;          // 0/1/2 depending on command
  uint8_t  flags;         // command-specific flags
  uint32_t group_id;      // host-generated group ID, 0 allowed for some commands
  uint32_t value;         // command-specific value
};
```

Commands:

| Command | Value | slot | flags | group_id | value |
|---|---:|---:|---|---:|---:|
| LINK | 1 | 1/2 | bit0=force | nonzero | reserved |
| UNLINK | 2 | 0 | bit0=force | current group or 0 | reserved |
| SET_ARMED | 3 | 0 | bit0=armed | any | reserved |
| IDENTIFY | 4 | 0 | reserved | any | duration_ms |
| FACTORY_RESET_LINK | 5 | 0 | bit0=force | 0 | reserved |

### LINK behavior

```text
if group_id == 0: reject
if slot not in [1,2]: reject
if already linked to different group and force == false: reject conflict
else set link_group_id=group_id, link_slot=slot, increment link_generation, persist
```

### UNLINK behavior

```text
if unlinked: ok no-op
if group_id == current group or group_id == 0 or force == true: clear link, increment link_generation, persist
else reject conflict
```

### SET_ARMED behavior

`armed` is runtime state. It may be reset to default on reboot. Initial default is `true` for simple operation.

### IDENTIFY behavior

Device flashes LED/display for `value` milliseconds. `value == 0` or `value > 30000` uses the default duration of 3000ms.

### FACTORY_RESET_LINK behavior

Clears `link_group_id` and `link_slot` and increments `link_generation`, so hosts observe the reset as a link change (`link_generation` is monotonic across all link mutations, including resets). This does not erase firmware or device_id.

### Receiver validation policy

v1 receivers validate only the fields each command defines:

```text
slot      validated for LINK only; other commands ignore it
group_id  validated for LINK and UNLINK only; SET_ARMED, IDENTIFY,
          and FACTORY_RESET_LINK ignore it
value     used by IDENTIFY only; other commands ignore it
flags     only bit0 is defined; receivers ignore other bits
```

Hosts must send 0 in ignored fields.

## 10. ControlResult characteristic

Format: UTF-8 JSON string. Sent after every Control write.

Example success:

```json
{
  "v": 1,
  "ok": true,
  "cmd": 1,
  "message": "linked",
  "device_id": "A1B2C3D4E5F6",
  "link_group_id": 12345678,
  "link_slot": 1,
  "link_generation": 5
}
```

Example conflict:

```json
{
  "v": 1,
  "ok": false,
  "cmd": 1,
  "error": "link_conflict",
  "message": "already linked to another group; use force"
}
```

## 11. Host link model

### 11.1 Host-side data

Host persists:

```ts
type LinkedPair = {
  groupId: number;
  slot1?: { deviceId: string; deviceHash: number; displayName: string };
  slot2?: { deviceId: string; deviceHash: number; displayName: string };
};
```

The Host should not rely on BLE MAC addresses because mobile OSes may expose per-app identifiers instead of raw BLE addresses.

### 11.2 Link workflow

```text
1. Host scans for DualStartButtonService.
2. User selects a Device.
3. Host connects and reads DeviceInfo.
4. User assigns Device to slot 1 or slot 2.
5. Host writes LINK command with group_id and slot.
6. Device persists link info and notifies ButtonState type=link.
7. Host stores device_id in local app storage.
```

### 11.3 Replacing one device

```text
1. Host opens link management UI.
2. User disconnects/removes broken slot locally.
3. User connects replacement Device.
4. Host writes LINK with same group_id and target slot.
5. If replacement Device is already linked elsewhere, Host asks for force confirmation.
6. Host overwrites slot record locally.
```

### 11.4 Replacing phone

```text
1. New phone scans all Devices.
2. User connects desired two Devices.
3. If Devices are already linked to old group, user uses force link.
4. New phone writes LINK to slot 1/2 with new or restored group_id.
5. Old phone state is irrelevant.
```

### 11.5 Unlinking

Unlink should exist in both layers.

- Local unlink: Host removes the device from slot but does not write to Device.
- Device unlink: Host writes UNLINK or FACTORY_RESET_LINK to clear Device-side link indicator.

For field operation, the UI should expose both:

```text
Remove from this phone
Unlink device
Force relink as slot 1/2
```

## 12. Host start condition

Recommended host state:

```ts
type ButtonRuntimeState = {
  connected: boolean;
  deviceId: string;
  deviceHash: number;
  groupId: number;
  slot: 1 | 2;
  pressed: boolean;
  armed: boolean;
  seq: number;
  lastReceivedAt: number;
};

// Host-derived Active per slot. Not on the wire.
type SlotActive = {
  active: boolean;
  prevPressed: boolean | null; // null until the first observed sample (baseline)
};
```

Recommended logic:

```ts
const STALE_MS = 1500;

const INITIAL_ACTIVE: SlotActive = { active: false, prevPressed: null };

// Feed every observed ButtonState sample of the device linked to the slot.
function reduceActive(tracker: SlotActive, pressed: boolean): SlotActive {
  const downEdge = tracker.prevPressed === false && pressed === true;
  return { active: downEdge ? !tracker.active : tracker.active, prevPressed: pressed };
}

function canStart(
  now: number,
  a: ButtonRuntimeState,
  b: ButtonRuntimeState,
  aActive: boolean,
  bActive: boolean,
): boolean {
  if (!a.connected || !b.connected) return false;
  if (!a.armed || !b.armed) return false;
  if (now - a.lastReceivedAt > STALE_MS) return false;
  if (now - b.lastReceivedAt > STALE_MS) return false;
  return aActive && bActive;
}
```

Active rules:

- 最初に観測したサンプルはbaselineでありトグルしない。接続時に押しっぱなしのボタンでactiveは変化しない。
- disconnect / unlink / (force) relink / new group作成で、そのslotの`SlotActive`を`INITIAL_ACTIVE`に戻す。
- staleはactiveを保持したまま開始をブロックし、freshに戻れば判定に復帰する。
- down通知を1回失っても、押下が継続していれば次のheartbeatのpressed=trueがdown-edgeとして観測される。downとupの両方を失った短い押下は取りこぼしうるため、hostは手動のactiveリセット操作を回復手段として提供する。

On disconnect:

```text
connected=false
pressed=false
active=false（baselineもクリア）
```

## 13. Reconnection policy

Device behavior:

- BLE server remains connectable after disconnect.
- On disconnect, advertising restarts.
- Current state remains readable through ButtonState.
- `seq` continues increasing until reboot.

Host behavior:

- Reconnect by stored `device_id` where the native app can preserve OS-level peripheral identifiers.
- If automatic reconnect fails, show manual scan UI.
- Never start if either slot is stale or disconnected.

## 14. Security and safety

Initial design intentionally avoids hard BLE bonding to keep replacement easy.

Risk:

- Nearby attacker or accidental user could connect and write Control.

Mitigations for initial field use:

- Use obscure 128-bit Service UUID.
- Show identify LED/display before force link.
- Require explicit UI confirmation for force link.
- Keep Control commands small and auditable.
- Optionally add app-layer shared secret/HMAC in v2 if operating in hostile public space.

For production in public spaces, introduce:

```text
ControlCommandV2:
  session_nonce
  command_nonce
  hmac_truncated_32 or 64
```

Do not use this initial v1 in a high-adversary environment without additional protection.

## 15. Firmware project

Location:

```text
firmware/pio
```

Primary dependencies:

- M5Unified for M5Stack family hardware abstraction.
- NimBLE-Arduino for BLE Peripheral implementation.
- Preferences for persistent link info.

Build targets are defined in `platformio.ini`.

Expected commands:

```bash
cd firmware/pio
pio run -e m5atom_lite
pio run -e m5stick_c
pio run -e m5stack_atoms3
pio run -e generic_esp32_gpio
```

Upload example:

```bash
pio run -e m5atom_lite -t upload
pio device monitor -b 115200
```

## 16. Debug web app

Location:

```text
web-debug
```

Purpose:

- Connect to arbitrary n devices.
- Read DeviceInfo.
- Subscribe ButtonState notifications.
- Link any connected device to slot 1/2.
- Unlink or force relink devices.
- Display live active/pressed/armed/stale state.
- Show whether the two selected slots satisfy the start condition.
- Reset the host-side active state manually.

Run locally:

```bash
cd web-debug
python3 -m http.server 8080
```

Open:

```text
http://localhost:8080
```

Web Bluetooth requires a compatible browser and a secure context. `localhost` is acceptable for local debug in Chromium-based browsers.

## 17. Test plan

### 17.1 Firmware unit-level/manual checks

| Case | Expected |
|---|---|
| Boot | Device advertises as `DSB-xxxx` |
| Read DeviceInfo | valid JSON returned |
| Subscribe ButtonState | state packet length is 20 bytes |
| Press button | pressed flag true, seq increments |
| Release button | pressed flag false, seq increments, aux includes hold_ms |
| Heartbeat | packet sent every ~1000ms |
| LINK slot 1 | link_group_id and link_slot persisted |
| Reboot after link | DeviceInfo still shows same link |
| UNLINK | link_group_id=0, link_slot=0 |
| Already linked + no force | conflict result |
| Already linked + force | link overwritten |
| Disconnect host | advertising restarts |

### 17.2 Debug web app checks

| Case | Expected |
|---|---|
| Add Device | Device card appears |
| Add 3+ Devices | All connected cards visible |
| Link one to slot1 | Slot1 shows selected device, active=no |
| Link another to slot2 | Slot2 shows selected device, active=no |
| Press slot1 once | Slot1 active=YES, start condition false |
| Press slot1 again | Slot1 active=no, start condition false |
| Press slot1 once, then slot2 once | Both active=YES, start condition true（同時押し不要） |
| Release both buttons | Start condition remains true（activeは保持） |
| Press slot1 again while ready | Slot1 active=no, start condition false |
| Reset Active | Both slots active=no, start condition false |
| Hold button while connecting | First observed state is a baseline, no toggle |
| Disconnect one active slot | Its active resets, start condition false, stale/connected state shown |
| Reconnect and press once | Slot becomes active again |
| Force relink | Device moves to selected slot, that slot's active resets |

### 17.3 Field operation checks

- 2台を1m、5m、10mで押下確認。
- スマホ画面ロック時・復帰時の挙動確認。実アプリがforeground前提なら明記する。
- 長時間待機後のdisconnect/reconnect確認。
- TailBAT満充電からの連続稼働時間確認。
- 交換手順を非開発者が実施できるか確認。

## 18. References

- M5 Atom Lite documentation: https://docs.m5stack.com/en/core/ATOM%20Lite
- M5Unified overview/button API: https://docs.m5stack.com/en/arduino/m5unified/helloworld and https://docs.m5stack.com/ja/arduino/m5unified/button_class
- M5Unified LED API: https://docs.m5stack.com/en/arduino/m5unified/led_class
- NimBLE-Arduino documentation: https://h2zero.github.io/NimBLE-Arduino/
- Web Bluetooth specification/community draft: https://webbluetoothcg.github.io/web-bluetooth/
- MDN startNotifications: https://developer.mozilla.org/en-US/docs/Web/API/BluetoothRemoteGATTCharacteristic/startNotifications
