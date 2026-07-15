# FilaTrack — projectcontext

## Dit Project Specifiek

- **Huidig Doel**: FilaTrack klaarmaken om te delen. Nu bezig met **meertalige interface**
  (web + tablet, taalkeuze in Settings). Daarvoor: hernoemd van PandaTouch → FilaTrack,
  ESP32-firmware eruit, één repo.
- **Wat is FilaTrack**: bedien- en monitorpaneel voor een **Bambu Lab P1S**, draait op een
  **Android-tablet** (Samsung SM-T280, Android 5.1.1 / API 22, armeabi-v7a) + volledige
  **webinterface** op `http://<tablet-ip>:8080`. Losse **FilaTrack Scale** (ESP32-S3 + HX711)
  levert echte spoelgewichten. Kern: niet zomaar status, maar **hoeveel filament er nog op zit
  en wat een print kost**.
- **Bekende Issues / valkuilen**:
  - **Firmware 1.08 in cloud-modus blokkeert** temperatuur (M104/M140) en print-start.
    Jog, M106 (fan), M18, G1 E (extrude) en posities werken wél. Niet opnieuw proberen.
  - **LVGL montserrat heeft alleen ASCII** — `·`, `€`, `✓` worden tofu op de tablet.
    Gebruik "EUR", `\xC2\xB0` voor °, gewone koppeltekens. In de WEB-HTML mag alles wél.
  - **Tablet-dashboard is verticaal vol** (~476 van 480 designpixels). Een rij toevoegen
    duwt de helderheidsschuif eraf. Gebruik lege ruimte in een bestaande rij.
  - **Geen overlays over de headerknoppen** — dat maakte ze onbereikbaar (zie git 20b54e8).
  - `adb shell input tap` **bereikt deze SDL-app niet**; knoppen kun je er niet mee testen.
  - Tablet **doost tussen prints**; bij wakker worden faalt de FTP-handshake soms
    (mbedTLS "hs -80"). Daarom retry + herladen bij MQTT-reconnect.

## Voortgang / Laatste Stand

- **Datum laatste sessie**: 15-07-2026
- **Wat is af**: back-up/herstel + herinnering + SD-kopie · live printkosten · ntfy
  (print klaar mét modelfoto, mislukt, tekort, rol bijna op) · per-materiaal statistiek ·
  filament-balkjes · verbindingsbol · ETA + laagteller · actief slot · klok ·
  waarschuwingspill · uitgebreide historie (zoeken/filteren/sorteren/CSV) ·
  voorraadoverzicht + restlengte · diagnose-scherm · self-test · **hernoemd naar FilaTrack**
  (incl. datamigratie) · package `nl.filatrack.app` · ESP32-firmware verwijderd (2097 regels).
- **Waar we gebleven zijn**: net begonnen aan **meertalige interface** (web + tablet).
- **Ontwerp i18n** (afgesproken 15-07): teksten krijgen een **sleutel** (`dash.printing`);
  **EN + NL zitten ingebouwd** zodat het out-of-the-box werkt zonder bestanden; daarnaast
  laadt de app **losse taalbestanden** `/sdcard/filatrack_lang_<code>.conf` (`sleutel=vertaling`,
  zelfde stijl als de andere conf-bestanden) die ingebouwde teksten overschrijven of een
  nieuwe taal toevoegen — **een taal erbij = een bestand neerzetten, geen hercompilatie**.
  Eén set bestanden bedient zowel tablet als web (web haalt ze via `/lang`), dus vertalingen
  hoeven maar één keer. `lang=` in filatrack.conf, `/setcfg?lang=`, keuze in Settings.
- **Volgende stap**: `sim/android/lang.{h,cpp}` bouwen (ingebouwde EN/NL + loader + `T(key)`),
  daarna schermen omzetten (web eerst — dat zien buitenstaanders het eerst).
- **Openstaand aan Arno's kant**: kiosk opnieuw kiezen (Home → FilaTrack → Altijd; reset door
  de nieuwe package) · `tools\Backup instellen.bat` draaien voor de dagelijkse back-up ·
  `Eigen-bambu-hulp` archiveren op GitHub.
- **Later/optioneel**: droog-datum + herinnering bij rollen · model-foto op de tablet
  (web werkt al; LVGL PNG-uit-geheugen is versie-gevoelig — samen testen tijdens een print).

## Bouwen & testen

```powershell
# sync + build + install (JAVA_HOME is Android Studio1, niet Android Studio!)
X:\projects\panda\FilaTrack\android\sync_sources.ps1 -Jni C:\pt_android\app\jni
$env:JAVA_HOME="C:\Program Files\Android\Android Studio1\jbr"
cd C:\pt_android; .\gradlew.bat assembleDebug
adb install -r C:\pt_android\app\build\outputs\apk\debug\app-debug.apk
adb shell monkey -p nl.filatrack.app -c android.intent.category.LAUNCHER 1
adb logcat -s FILATRACK          # onze logs

X:\projects\panda\FilaTrack\tools\selftest.ps1    # 18 checks tegen de echte tablet
```

- **Scratch-boom `C:\pt_android`**: `Android.mk`, `build.gradle`, `AndroidManifest.xml` en
  `res/` zijn **aparte kopieën** — de sync kopieert die NIET. Nieuw bronbestand? Voeg het toe
  aan `android/sync_sources.ps1` (expliciete lijst) **én** aan beide `Android.mk`'s.
- **Tablet**: 192.168.2.110:8080 · **printer**: 192.168.2.27 · **scale**: 192.168.2.60

## Codeerrichtlijnen (aanvullend op de globale)

- **Taal**: communiceer in het Nederlands. Code/commits/comments in het Engels.
- **Nooit hardcoden**: de printer-**toegangscode** is een secret — alleen op de tablet in
  `/sdcard/filatrack.conf`, nooit in git. Staat in `.gitignore` (beide namen). De `/backup`
  bevat 'm bewust NIET. Geen secrets in Notion.
- **Data op de tablet**: `/sdcard/filatrack_*.conf`. Wijzig je een bestandsnaam of -formaat,
  **bouw dan een migratie** (zie `migrate_legacy_data()`) — anders start iedereen leeg op.
- **Werk op een feature-branch**, niet direct op main. Commit/push alleen op verzoek.
- **Niet destructief testen op echte data** (er zijn ooit echte spoelen gewist). De self-test
  gebruikt een wegwerp-rol die hij altijd opruimt.
- `[env:pandatouch]` in platformio.ini + `.pio/libdeps/pandatouch` **blijven zo heten** — die
  leveren PubSubClient/ArduinoJson voor de Android-build. Eén hernoemen breekt het stil.

## Repo

- Eén repo: **github.com/sankeeye/FilaTrack** (remote `origin`). `main` == de feature-branch.
- Alles is eigen werk; er zit geen code van derden meer in (gemeten: 19 gedeelde regels,
  allemaal kale declaraties). Zie "How it started" in de README.
