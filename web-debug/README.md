# Dual Start Button Debug Web App

Web BluetoothでDual Start Buttonデバイスに接続し、次を確認するための簡易ツールです。

- 任意のn台の接続
- DeviceInfoの確認
- ButtonState Notifyの購読
- slot 1 / slot 2へのlink
- unlink / force link
- identify / arm / disarm
- 2台同時押下によるstart condition確認

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

プロトコルのencode/decode (`protocol.js`) と開始条件の判定 (`start-condition.js`) は
Node.js組み込みのテストランナーで検証できます。依存パッケージやビルドは不要です。

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
