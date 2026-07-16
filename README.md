# FilaTrack for Bambu Lab

**English** · [Nederlands](README.nl.md) · [Deutsch](README.de.md)

A standalone **control panel and monitor for a Bambu Lab 3D printer** that runs on an
**Android tablet** — with a full web interface as well, so you can drive the printer from the
tablet or from any browser on your LAN. A companion **FilaTrack Scale** weigh scale adds real
spool‑weight tracking. See [Hardware](#hardware).

The interface is available in **English, Dutch and German**, switchable on the fly, and more
languages can be added by dropping in a file — see [`lang/`](lang/).

## Features

- **Live printer status & control** over the printer's **local MQTT** feed (the broker
  inside the printer, reached on your network with the LAN access code — this works in the
  printer's normal cloud mode too, no *"LAN Only"* mode required): print progress,
  temperatures, and pause/resume/stop/light controls.
- **Filament / AMS & spool‑weight tracking**: remaining grams that count down as a print
  runs, plus the cost of each print. No extra hardware required — see the filament manager below.
- **File browser** over the printer's **FTP** service.
- **Bambu Cloud integration** for the one thing the local MQTT feed never exposes: how many
  grams a finished print actually used.
- **LVGL touch UI** (LVGL 9.3): printer screen, filament/AMS, file browser, settings.
- **On‑screen setup** for the printer connection (IP / serial / LAN access code).

On the **Android‑tablet / web build**, additionally:

- **Manual motion** ("Move"): jog X/Y/Z, home, extrude/retract, and — in LAN mode —
  temperature presets.
- **Full web control** on `:8080` mirroring every tablet screen (dashboard, filament, files,
  move, scale, spools, settings), **password‑protected and answering only on your own network**.
- **Filament manager (no scale required)**: a spool library (create/edit/copy/search/bulk‑edit).
  Enter a roll's weight and the remaining grams tick down live during a print (the sliced file's
  total × progress), with price / cost / remaining‑value and a *"will this print run short?"*
  warning. The optional **FilaTrack Scale** lets you weigh instead of type — and re‑weigh for the
  exact figure — while the Bambu Cloud relay (below) can feed each print's real grams to keep it
  accurate without a scale.
- **ntfy push notifications** (print done/failed, filament short), **print history &
  statistics**, and kiosk crash‑restart.

> **Note — temperature & starting prints.** On recent Bambu firmware (≥ 01.08), the printer
> rejects *set‑temperature* and *start‑print* commands from third‑party tools unless it is in
> **LAN Only** mode. FilaTrack has a **LAN mode** switch (Settings ▸ Printer setup): turn it on
> when your printer is in LAN Only and the temperature controls and print‑start appear. In the
> normal cloud mode, leave it off and start prints from Bambu Studio / Handy — everything else
> here (monitoring, pause/stop, light, move/jog, AMS settings) works either way.

## Hardware

- **Android tablet** — runs the LVGL UI (`src/ui_*`) as a standalone printer
  monitor/controller with a live local‑MQTT link to the printer, **plus a full web UI** on
  `http://<tablet>:8080`. Verified on a Samsung SM‑T280 (Android 5.1.1).
  See [`android/README.md`](android/README.md).
- **FilaTrack Scale** — an ESP32‑S3 + HX711 load‑cell scale (the SpoolEase Scale hardware flashed
  with our own firmware) that feeds real spool weights to the tablet for filament tracking,
  cost, and low/short‑filament warnings. See [`scale/`](scale/).

Developed and tested on a **Bambu Lab P1S**. FilaTrack talks to the printer over the local
**MQTT + FTP** interface that Bambu Lab printers share, so other models (P1P, X1C, A1, …) should
work too — they just haven't been tested here yet.

The exact same `src/ui_*` code also builds as a **PC simulator** (`sim/`) for development.

## Build & flash

- **Android tablet** — build the APK with the Android NDK + Gradle and push it to the tablet.
  Full step‑by‑step in [`android/README.md`](android/README.md). The printer IP / serial /
  LAN access code go in `/sdcard/filatrack.conf` (see `sim/android/filatrack.conf.example`)
  or the on‑screen **Printer setup**.
- **FilaTrack Scale** — open `scale/` in **VS Code** with the **PlatformIO** extension and flash
  over **USB‑C** (environment `filatrack_scale`). First‑run WiFi setup and load‑cell calibration
  are described in `scale/`.

## Security

The web interface controls a real printer, so it is not left open:

- **Password.** The tablet generates a random password on first run and shows it under
  **Settings ▸ Web password** — like a TV showing a pairing code, so there is no default password
  to forget to change and each device gets its own. Sign in as user `filatrack`.
- **Local network only.** The server refuses connections from outside your own network, so a
  forwarded port or UPnP can't put the printer on the internet.
- The printer's LAN access code stays on the tablet (in `/sdcard/filatrack.conf`) and is never
  included in a backup or exposed by the web interface.

It is plain HTTP, so the password crosses your own LAN unencrypted — enough to keep the printer
off the internet and away from other devices on the Wi‑Fi, not a defence against someone actively
sniffing your network.

## Filament‑weight relay (`tools/`)

Bambu Cloud's login is behind Cloudflare bot‑protection, so a small Python helper runs on
your PC instead: it logs in to Bambu Cloud, watches your print history, and reports the grams
used by each finished print to the tablet over your LAN.

Setup:

1. `pip install requests`
2. Copy `tools/config.example.json` to `tools/config.json` and fill in your details
   (Bambu email, printer serial, the tablet's IP, port and web password - Settings > Web password on the tablet). `config.json` and `relay_state.json`
   are git‑ignored — they hold your credentials/token and must stay local.
3. Run it: `python tools/bambu_weight_relay.py` — leave it running while you print.

Notes:

- **Verification code**: about every 90 days a fresh login may need a 6‑digit code emailed to
  you; the relay asks for it in its terminal, so run it with a visible console for the first login.
- **Windows convenience**: `encrypt_password.bat` stores your password encrypted
  (`bambu_password_enc`, via Windows DPAPI tied to your account) instead of plaintext;
  `start_relay_hidden.vbs` runs the relay with no console window. These are optional and
  Windows‑only — on Linux/macOS just use a plaintext `bambu_password` in your (git‑ignored)
  `config.json`, or a `custom_token`.

## How it started

FilaTrack began as firmware for a small ESP32 handheld panel — somewhere to see what
the printer was doing without opening Bambu Studio on a laptop. It worked, but the
hardware ended up being the thing that limited it, and an old Android tablet turned
out to do the same job better: a bigger screen, WiFi the OS already takes care of,
and room to grow.

Grow it did — and the interesting part was not another status readout. It was the
question that actually comes up in the workshop: *how much filament is still on that
spool, and what did this print cost me?* Answering that honestly needs real weights,
not a percentage guess, so a second device joined in: an ESP32‑S3 with a load cell —
the FilaTrack Scale. From there came the spool library, cost per print, the history
and the statistics.

The ESP32 firmware has since been removed. What's left is the tablet app, its web
interface, and the scale.

## Credits

- Built on LVGL and SDL, plus the open‑source libraries listed in `platformio.ini`.

## License

MIT — see [LICENSE](LICENSE). The original project is also MIT; this fork keeps that.
