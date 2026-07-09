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

## Operational summary

1. Flash the same firmware to all button devices.
2. Open the debug app or the real smartphone app.
3. Connect any number of devices.
4. Link two chosen devices to slot 1 and slot 2.
5. Each press of a linked button toggles its slot's host-side active state. Start condition becomes true when both linked slots are connected, armed, fresh, and active. Simultaneous pressing is not required.
6. To replace a device or phone, scan again and force link the desired devices.
