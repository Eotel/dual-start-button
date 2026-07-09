# Dual Start Button Debug Web App

Web BluetoothでDual Start Buttonデバイスに接続し、次を確認するための簡易ツールです。

- 任意のn台の接続
- DeviceInfoの確認
- ButtonState Notifyの購読
- slot 1 / slot 2へのlink
- unlink / force link
- identify / arm / disarm
- トグル式start condition（押すたびslotのactiveが反転し、両slot activeで成立）の確認
- host側activeの手動リセット
- iPhone browser向けHID keyboard fallbackのlocal slot assignment確認

## Run

```bash
cd web-debug
python3 -m http.server 8080
```

Open:

```text
http://localhost:8080
```

Web Bluetoothは対応ブラウザとsecure contextが必要です。ローカルdebugでは `localhost` を使ってください。

## Tests

プロトコルのencode/decode (`protocol.js`)、activeトグル (`active-state.js`)、
開始条件の判定 (`start-condition.js`)、HID fallback state (`hid-fallback.js`)
はNode.js組み込みのテストランナーで検証できます。
依存パッケージやビルドは不要です。

```bash
cd web-debug
node --test
```

リポジトリルートから実行する場合は glob で指定してください。

```bash
node --test 'web-debug/**/*.test.js'
```

`protocol.test.js` は `../test-vectors/` のgolden vectorを読み込み、firmware / Python /
C++と同じ契約を確認します。

## Notes

- `Add / Connect Device` はブラウザのBluetooth chooserを開きます。n台接続したい場合は複数回押してください。
- `Reconnect Granted Devices` は対応ブラウザでのみ動きます。
- `force link/unlink` は既に別groupにlinkされているデバイスを上書きするときに使います。
- `New Group ID` はこのWeb appのローカルgroupを作り直します。Device側のlink状態は消しません。
- `Reset Active (both slots)` はhost側のactive状態を両slotともリセットします。Device側の状態は変わりません。
- `HID Keyboard Fallback` はBLE GATT linkではありません。OS/browserが出すkeyboard eventをlocalStorageのslot bindingに割り当てるだけです。
- HID fallbackにはDeviceInfo、heartbeat、disconnect検知、OTA、link/unlink、identify、arm/disarmはありません。通常運用とAndroid/Desktop ChromeではGATT側を使ってください。
