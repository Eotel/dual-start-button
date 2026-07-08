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

## Notes

- `Add / Connect Device` はブラウザのBluetooth chooserを開きます。n台接続したい場合は複数回押してください。
- `Reconnect Granted Devices` は対応ブラウザでのみ動きます。
- `force link/unlink` は既に別groupにlinkされているデバイスを上書きするときに使います。
- `New Group ID` はこのWeb appのローカルgroupを作り直します。Device側のlink状態は消しません。
