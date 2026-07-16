# FilaTrack on an Android tablet

Runs the **same LVGL UI** as the PC simulator in [`../sim`](../sim) on an Android
tablet, as a dedicated Bambu-printer monitor. The UI talks to the printer for real:
a live **LAN MQTT/TLS** session (status + control) via `bambu_mqtt.cpp`.

Verified on a **Samsung SM-T280** (Android 5.1.1 / API 22, armeabi-v7a).

## How it fits together

This code began life as ESP32/Arduino firmware. That device firmware has since been
removed, but the PC sim and this Android app still compile the same `src/ui_*.cpp`
screens and the Arduino-shaped `bambu_mqtt.cpp` unchanged — small shims stand in for
what the Arduino core used to provide:

| Concern            | Original (ESP32/Arduino) | Android tablet                                 |
|--------------------|------------------------|--------------------------------------------------|
| Display + touch    | RGB panel + GT911      | SDL2 (LVGL's `LV_USE_SDL`), fullscreen           |
| `Arduino.h`, `WiFi`| Arduino core           | shims in [`../sim/shim`](../sim/shim)            |
| MQTT transport     | `WiFiClientSecure`     | `sim/shim/WiFiClientSecure.h` (mbedTLS-backed)   |
| MQTT client        | PubSubClient           | same PubSubClient (from `.pio/libdeps`)          |
| Storage / settings | LittleFS + NVS         | `sim/android/android_glue.cpp` reads a conf file |
| Printer IP/serial/code | webserver + NVS    | `/sdcard/filatrack.conf` (see below)            |

`bambu_mqtt.cpp`, `PubSubClient.cpp` and `ArduinoJson` are compiled **unchanged**;
the Arduino `Print`/`Stream`/`Client`/`IPAddress` base classes and a real
mbedTLS `WiFiClientSecure` (TLS handshake, `setInsecure()` since the printer serves
a self-signed LAN cert) live in `../sim/shim`.

## What's in git vs. what you fetch

In git (this folder) — only the files we authored:

```
android/
  app/build.gradle                     compileSdk 34, ndk 25.2.9519653, armeabi-v7a
  app/src/main/AndroidManifest.xml     kiosk (HOME) + INTERNET + storage perms
  app/src/main/res/values/strings.xml  app name
  app/jni/Application.mk                APP_ABI, APP_PLATFORM=android-19, APP_SHORT_COMMANDS
  app/jni/Android.mk                    top-level (includes the module makefiles)
  app/jni/lv_conf.h                     LVGL config (LV_USE_SDL=1, fullscreen)
  app/jni/lvgl/Android.mk               builds liblvgl.a
  app/jni/mbedtls/Android.mk            builds libmbedtls.a (library/ only, see gotchas)
  app/jni/src/Android.mk                the app module: ourui + bambu_mqtt + PubSubClient
  app/jni/src/android_compat.c          aligned_alloc shim for API < 28
  settings.gradle, gradle.properties, gradle/wrapper/gradle-wrapper.properties
  sync_sources.ps1                      copies repo sources into a jni/ tree
```

NOT in git — reconstruct a build tree (e.g. `C:\pt_android`) from the SDL2
Android project template, then drop in:

- `app/jni/SDL/`      — SDL2 source (SDL 2.x release, the Android project layout)
- `app/jni/lvgl/`     — LVGL 9.3 source (same version as the firmware) + our `Android.mk`
- `app/jni/mbedtls/`  — mbedTLS **3.6.2** source + our `Android.mk`
- our authored files above, copied over the template
- the OUR sources + PubSubClient/ArduinoJson via `sync_sources.ps1`

## Build

Toolchain: Android SDK (platform 34), **NDK r25c** (`25.2.9519653`), Gradle 8.5,
a JDK (Android Studio's JBR is fine).

```powershell
.\android\sync_sources.ps1 -Jni C:\pt_android\app\jni
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
$env:ANDROID_HOME = "$env:LOCALAPPDATA\Android\Sdk"
cd C:\pt_android
gradle assembleDebug           # -> app/build/outputs/apk/debug/app-debug.apk
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

## Configure the tablet (printer connection)

**On-screen (normal way):** the app opens a **Printer setup** form on first boot
(and any time the access code is missing), and it's always reachable via
**Settings → Printer setup**. Type the printer IP, serial and access code on the
on-screen keyboard, tap **Save & connect**. It writes `/sdcard/filatrack.conf`
and reconnects immediately. The access code (printer screen: Settings > WLAN >
Access Code) is a secret — it lives only on the tablet, never in git.

**Or push the file** (headless):

```
printer_ip=192.168.2.27
printer_serial=01P00Cxxxxxxxxx
access_code=xxxxxxxx
```

```powershell
adb push filatrack.conf /sdcard/filatrack.conf
```

Restart the app. Watch it connect:

```powershell
adb logcat -s FILATRACK
# BAMBU: Connecting to 192.168.2.27 ...
# (wrong code -> "MQTT connect failed, state=5"; correct code -> connected, live data)
```

## Gotchas (all already handled in the checked-in files)

- **armeabi-v7a + mbedTLS**: the bundled Everest/p256-m crypto uses `__int128`,
  which 32-bit ARM lacks. `mbedtls/Android.mk` compiles only `library/*.c`
  (Everest is off in the config anyway) so mbedTLS falls back to its generic ECP.
- **`APP_SHORT_COMMANDS := true`** in `Application.mk` — LVGL's ~400 objects blow
  past the Windows command-line length limit when `ar` links `liblvgl.a`.
- **`aligned_alloc`** entered Android libc at API 28; LVGL's SDL driver calls it,
  so `android_compat.c` provides it via `memalign` for older devices.
- **Screen must be awake AND unlocked** for the app to start: SDL only runs
  `SDL_main` once the activity is resumed with a valid surface. On a locked/asleep
  tablet it goes straight to `onStop` and the UI never starts. For appliance use,
  set Lock screen = None and a long screen timeout (`svc power stayon true` keeps
  it on over USB while developing).
- **`adb screencap` shows the UI as black** — it can't capture SDL's hardware GL
  surface. Look at the physical screen, not the screenshot.
- **UTF-8 without BOM** for `build.gradle` / `lv_conf.h`: a PowerShell-added BOM
  makes Groovy/clang choke. Write these with `UTF8Encoding($false)`.
- **`lv_conf.h` uses `LV_USE_STDLIB_STRING = LV_STDLIB_BUILTIN`** (not CLIB): on
  Android 5.1 (API 22, 32-bit) bionic's `strnlen(s, SIZE_MAX)` misbehaves, and
  LVGL's text draw calls `lv_strndup(text, LV_TEXT_LEN_MAX)`. With the C-lib
  path this overran and crashed (SIGSEGV in `lv_draw_label`) the moment a
  keyboard/label was drawn. LVGL's built-in string funcs use a plain loop and
  are safe. Changing this recompiles all of LVGL (~90 s).
