# Web Flasher

Static ESP Web Tools page for installing Dual Start Button firmware over USB.

Generate local artifacts after building firmware:

```bash
devenv shell -- pio run -e m5stick_c -d firmware/pio
devenv shell -- python3 tools/prepare_web_flasher.py --env m5stick_c --tag local
python3 -m http.server 18080 -d dist/web-flasher/m5stick_c
```

Open `http://localhost:18080` in Chrome or Edge and choose the connected ESP32 serial port.

The generated directory contains:

```text
index.html
app.js
style.css
manifest.json
dual-start-button-m5stick_c-<tag>-merged.bin
```
