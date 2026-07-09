# Dual Start Button

M5Stack family devicesをBLE GATT Peripheralとして動作させ、スマホ側で任意のn台から2台を選んでトグル式（各ボタンを押すたびにactiveが反転し、両方activeで開始）の開始条件を作るための仕様・firmware・debug web appです。

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
pio run -e m5stick_c_hid
pio run -e m5stack_atoms3
pio run -e generic_esp32_gpio
```

M5StickC Plus is currently built with the `m5stick_c` env:

```bash
devenv shell -- pio run -e m5stick_c -d firmware/pio
devenv shell -- pio run -e m5stick_c -d firmware/pio -t upload --upload-port /dev/cu.usbserial-XXXXXXXX
devenv shell -- pio device monitor -d firmware/pio -b 115200 --port /dev/cu.usbserial-XXXXXXXX
```

### Browser USB flasher

For non-developer flashing over USB, generate an ESP Web Tools bundle from a
built PlatformIO image:

```bash
devenv shell -- pio run -e m5stick_c -d firmware/pio
devenv shell -- python3 tools/prepare_web_flasher.py --env m5stick_c --tag local
python3 -m http.server 18080 -d dist/web-flasher/m5stick_c
```

Open `http://localhost:18080` in Chrome or Edge and select the connected serial
port. Hosted copies must be served over HTTPS because browser serial access
requires a secure context.

To build a browser flasher bundle for the iPhone HID keyboard fallback image:

```bash
devenv shell -- pio run -e m5stick_c_hid -d firmware/pio
devenv shell -- python3 tools/prepare_web_flasher.py --env m5stick_c_hid --tag local
python3 -m http.server 18081 -d dist/web-flasher/m5stick_c_hid
```

### Debug app

```bash
cd web-debug
python3 -m http.server 8080
```

Open `http://localhost:8080` in a Web Bluetooth compatible browser.

The debug app also includes a separate HID Keyboard Fallback section for iPhone
browser testing. HID fallback uses OS keyboard events and local browser slot
bindings; it is not the BLE GATT protocol and does not provide DeviceInfo,
heartbeat, link/unlink, identify, arm/disarm, or OTA. See
`docs/hid-fallback.md`.

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

## Development environment

開発ツールチェーンと git hooks は [devenv](https://devenv.sh) が供給します。設定の単一ソースは `devenv.nix` です。

```bash
devenv shell   # 初回エントリ時に hooks (pre-commit / commit-msg / pre-push) が自動インストールされる
devenv test    # 全ファイルに対して lint / format / テスト / 静的解析を一括実行
```

| Stage | 内容 |
| --- | --- |
| pre-commit | nixfmt / ruff / biome(JS/JSON)/ clang-format による format と lint、hygiene チェック、actionlint、secrets 検出 |
| commit-msg | gitlint による conventional commits 検証(type: feat/fix/refactor/docs/test/chore/perf/ci、subject は小文字始まり) |
| pre-push | `pytest tools/`、web-debug の `node --test`、`pio test -e native`、`pio check`(cppcheck) |

CI の `lint` ジョブがローカルと同一の hook 一式を全ファイルに対して実行するため、hooks をバイパスした push も CI が検出します。

## Releases

タグ `v*` を push すると GitHub Actions がGATT 4環境と `m5stick_c_hid` のファームウェアをビルドし、GitHub Release に `dual-start-button-<env>-<tag>.bin`(アプリイメージ)と `SHA256SUMS` を添付します。

```bash
git tag v0.2.0 && git push origin v0.2.0
```

- タグを打つ前に `firmware/pio/platformio.ini` の `DSB_FW_VERSION` をタグと一致させてください(DeviceInfo が報告するバージョンの源)。
- 添付の bin はアプリイメージ(オフセット 0x10000)で、書き込み済みデバイスの更新用です。工場出荷状態のデバイスへの初回書き込み(bootloader / partition table を含む)は `pio run -e <env> -t upload` を使ってください。
- Release には `dual-start-button-web-flasher-m5stick_c-<tag>.zip` と `dual-start-button-web-flasher-m5stick_c_hid-<tag>.zip` も添付します。前者は通常GATT、後者はiPhone HID fallback向けです。zip を展開して HTTPS または localhost で配信すると、初回書き込みや復旧に使えます。

## Update path

現在の簡単な書き換え導線は、USB 接続 + browser flasher です。BLE OTA は次段階で、partition table、rollback、checksum/signature、転送中の BLE service 方針を決めてから実装します。詳細は `docs/adr/0001-firmware-update-path.md` を参照してください。

## Operational summary

1. Flash the same firmware to all button devices.
2. Open the debug app or the real smartphone app.
3. Connect any number of devices.
4. Link two chosen devices to slot 1 and slot 2.
5. Each press of a linked button toggles its slot's host-side active state. Start condition becomes true when both linked slots are connected, armed, fresh, and active. Simultaneous pressing is not required.
6. To replace a device or phone, scan again and force link the desired devices.
