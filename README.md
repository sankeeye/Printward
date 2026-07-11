# PandaTouch for Bambu Lab P1S

Firmware that turns a **BigTreeTech PandaTouch** (ESP32‑S3 touchscreen module) into a
standalone **control panel and monitor for a Bambu Lab P1S** 3D printer — no PC required.
The same UI also runs on a **PC simulator** and — the way it's mainly used now — on an
**Android tablet** with a full web interface (see [Hardware & where it runs](#hardware--where-it-runs)).

> This is a Bambu‑printer fork of the excellent
> [PandaTouch StreamDeck by Disttrack](https://github.com/Disttrack/PandaTouch_streamDeck).
> The original turns the PandaTouch into a Bluetooth StreamDeck; this project reuses its
> hardware/display/web/OTA foundation and replaces the StreamDeck logic with Bambu
> printer monitoring and control. See [Credits](#credits).

## Features

- **Live printer status & control** over the printer's local (LAN‑only) **MQTT** feed:
  print progress, temperatures, and pause/resume/stop/light controls.
- **Filament / AMS & spool weight tracking**, including automatic subtraction of the
  grams used by each finished print (see the weight relay below).
- **File browser** over the printer's **FTP** service.
- **Bambu Cloud integration** for the one thing LAN‑only MQTT never exposes: how many
  grams a finished print actually used.
- **LVGL touch UI** (LVGL 9.3): printer screen, filament/AMS, file browser, settings.
- **WiFi setup**, a **web configuration dashboard**, and **OTA** firmware updates.

On the **Android‑tablet / web build**, additionally:

- **Manual motion** ("Move"): jog X/Y/Z, home, extrude/retract, preheat/cooldown.
- **Full web control** on `:8080` mirroring every tablet screen (dashboard, filament, files,
  move, scale, spools, settings).
- **Scale‑based filament manager**: a spool library (create/edit/copy/search/bulk‑edit),
  weigh spools with the **PandaScale**, live remaining grams that tick down during a print,
  price / cost / remaining‑value, and a *"will this print run short?"* warning.
- **ntfy push notifications** (print done/failed, filament short), **print history &
  statistics**, and kiosk crash‑restart.

## Hardware & where it runs

The same LVGL UI (`src/ui_*`) runs on **three targets**:

- **BigTreeTech PandaTouch** — ESP32‑S3, RGB parallel panel, GT911 capacitive touch,
  16 MB flash, PSRAM. The original device, built with PlatformIO (env `pandatouch`).
- **Android tablet** — the same UI as a standalone printer monitor/controller with a live
  LAN‑MQTT link, **plus a full web UI** on `http://<tablet>:8080`. This is how the project
  is mainly used now (the physical PandaTouch here runs stock BTT firmware). Verified on a
  Samsung SM‑T280 (Android 5.1.1). See [`android/README.md`](android/README.md).
- **PC simulator** — the UI in an SDL window for fast iteration (`sim/`).

Optional companion hardware:

- **PandaScale** — an ESP32‑S3 + HX711 load‑cell scale (the SpoolEase Scale hardware flashed
  with our own firmware) that feeds real spool weights to the tablet for filament tracking,
  cost, and low/short‑filament warnings. See [`scale/`](scale/).

## Build & flash (PlatformIO)

The project uses a custom partition layout, so the **first** install must be done over
**USB‑C** (after that, updates can be done wirelessly via OTA from the web dashboard).

1. Open this folder in **VS Code** with the **PlatformIO** extension.
2. Connect the PandaTouch via **USB‑C** (install the CH340 serial driver if needed).
3. Click the PlatformIO **Upload** button (or run `pio run -t upload`). Environment:
   `pandatouch`.
4. On the device screen, note the IP address and open `http://<device-ip>/` to configure
   WiFi, the printer connection (IP, serial, LAN access code), and more.

## Filament‑weight relay (`tools/`)

Bambu Cloud's login is behind Cloudflare bot‑protection that blocks the ESP32's TLS
fingerprint, so the device can't log in by itself. A small Python helper runs on your PC
instead: it logs in to Bambu Cloud, watches your print history, and reports the grams used
by each finished print to the PandaTouch over your LAN. It also keeps the device's own
cloud token fresh.

Setup:

1. `pip install requests`
2. Copy `tools/config.example.json` to `tools/config.json` and fill in your details
   (Bambu email, printer serial, the device's IP). `config.json` and `relay_state.json`
   are git‑ignored — they hold your credentials/token and must stay local.
3. Run it: `python tools/bambu_weight_relay.py` — leave it running while you print.

Notes:

- **Verification code**: when Bambu asks for a 6‑digit email code, the relay shows a field
  in the PandaTouch web dashboard where you type it — so the relay can run headless.
- **Windows convenience**: `encrypt_password.bat` stores your password encrypted
  (`bambu_password_enc`, via Windows DPAPI tied to your account) instead of plaintext;
  `start_relay_hidden.vbs` runs the relay with no console window. These are optional and
  Windows‑only — on Linux/macOS just use a plaintext `bambu_password` in your (git‑ignored)
  `config.json`, or a `custom_token`.

## Credits

- Based on **[PandaTouch StreamDeck](https://github.com/Disttrack/PandaTouch_streamDeck)**
  by Disttrack (MIT). Huge thanks for the display, web dashboard and OTA groundwork.
- Built on LVGL, the Arduino‑ESP32 framework, and the open‑source libraries listed in
  `platformio.ini`.

## License

MIT — see [LICENSE](LICENSE). The original project is also MIT; this fork keeps that.
