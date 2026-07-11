# PandaTouch for Bambu Lab P1S

A standalone **control panel and monitor for a Bambu Lab P1S** 3D printer that runs on an
**Android tablet** — with a full web interface as well, so you can drive the printer from the
tablet or from any browser on your LAN. A companion **PandaScale** weigh scale adds real
spool‑weight tracking. See [Hardware](#hardware).

> This is a Bambu‑printer fork of the excellent
> [PandaTouch StreamDeck by Disttrack](https://github.com/Disttrack/PandaTouch_streamDeck).
> The original turns the PandaTouch into a Bluetooth StreamDeck; this project started from
> its LVGL / web / OTA groundwork and replaced the StreamDeck logic with Bambu printer
> monitoring and control — now running on an Android tablet. See [Credits](#credits).

## Features

- **Live printer status & control** over the printer's **local MQTT** feed (the broker
  inside the P1S, reached on your network with the LAN access code — this works in the
  printer's normal cloud mode too, no *"LAN Only"* mode required): print progress,
  temperatures, and pause/resume/stop/light controls.
- **Filament / AMS & spool weight tracking**, including automatic subtraction of the
  grams used by each finished print (see the weight relay below).
- **File browser** over the printer's **FTP** service.
- **Bambu Cloud integration** for the one thing the local MQTT feed never exposes: how many
  grams a finished print actually used.
- **LVGL touch UI** (LVGL 9.3): printer screen, filament/AMS, file browser, settings.
- **On‑screen setup** for the printer connection (IP / serial / LAN access code).

On the **Android‑tablet / web build**, additionally:

- **Manual motion** ("Move"): jog X/Y/Z, home, extrude/retract, preheat/cooldown.
- **Full web control** on `:8080` mirroring every tablet screen (dashboard, filament, files,
  move, scale, spools, settings).
- **Scale‑based filament manager**: a spool library (create/edit/copy/search/bulk‑edit),
  weigh spools with the **PandaScale**, live remaining grams that tick down during a print,
  price / cost / remaining‑value, and a *"will this print run short?"* warning.
- **ntfy push notifications** (print done/failed, filament short), **print history &
  statistics**, and kiosk crash‑restart.

> **Note — starting prints.** On P1S firmware ≥ 01.08, Bambu rejects *start‑print* commands
> from third‑party tools (error `0x05024007`) unless the printer is in **LAN Only** or
> **Developer** mode. So kick prints off from Bambu Studio / Handy; everything else here
> (monitoring, pause/stop, light, move/jog, AMS settings) is unaffected and works in normal
> cloud mode.

## Hardware

- **Android tablet** — runs the LVGL UI (`src/ui_*`) as a standalone printer
  monitor/controller with a live local‑MQTT link to the printer, **plus a full web UI** on
  `http://<tablet>:8080`. Verified on a Samsung SM‑T280 (Android 5.1.1).
  See [`android/README.md`](android/README.md).
- **PandaScale** — an ESP32‑S3 + HX711 load‑cell scale (the SpoolEase Scale hardware flashed
  with our own firmware) that feeds real spool weights to the tablet for filament tracking,
  cost, and low/short‑filament warnings. See [`scale/`](scale/).

The exact same `src/ui_*` code also builds as a **PC simulator** (`sim/`) for development.

## Build & flash

- **Android tablet** — build the APK with the Android NDK + Gradle and push it to the tablet.
  Full step‑by‑step in [`android/README.md`](android/README.md). The printer IP / serial /
  LAN access code go in `/sdcard/pandatouch.conf` (see `sim/android/pandatouch.conf.example`)
  or the on‑screen **Printer setup**.
- **PandaScale** — open `scale/` in **VS Code** with the **PlatformIO** extension and flash
  over **USB‑C** (environment `pandascale`). First‑run WiFi setup and load‑cell calibration
  are described in `scale/`.

## Filament‑weight relay (`tools/`)

Bambu Cloud's login is behind Cloudflare bot‑protection, so a small Python helper runs on
your PC instead: it logs in to Bambu Cloud, watches your print history, and reports the grams
used by each finished print to the tablet over your LAN. It also keeps the tablet's own cloud
token fresh.

Setup:

1. `pip install requests`
2. Copy `tools/config.example.json` to `tools/config.json` and fill in your details
   (Bambu email, printer serial, the tablet's IP). `config.json` and `relay_state.json`
   are git‑ignored — they hold your credentials/token and must stay local.
3. Run it: `python tools/bambu_weight_relay.py` — leave it running while you print.

Notes:

- **Verification code**: when Bambu asks for a 6‑digit email code, the relay shows a field
  in the tablet's web dashboard where you type it — so the relay can run headless.
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
