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
- **Waar we gebleven zijn**: **de web-interface is helemaal vertaald** (EN/NL/DE compleet,
  264 sleutels). De tablet-UI staat nog in het Nederlands — dat is de volgende stap.
- **Ontwerp i18n** (afgesproken 15-07): teksten krijgen een **sleutel** (`dash.printing`);
  **EN + NL zitten ingebouwd** zodat het out-of-the-box werkt zonder bestanden; daarnaast
  laadt de app **losse taalbestanden** `/sdcard/filatrack_lang_<code>.conf` (`sleutel=vertaling`,
  zelfde stijl als de andere conf-bestanden) die ingebouwde teksten overschrijven of een
  nieuwe taal toevoegen — **een taal erbij = een bestand neerzetten, geen hercompilatie**.
  Eén set bestanden bedient zowel tablet als web (web haalt ze via `/lang`), dus vertalingen
  hoeven maar één keer. `lang=` in filatrack.conf, `/setcfg?lang=`, keuze in Settings.
- **Volgende stap**: de **tablet-UI** omzetten naar `T("sleutel")` (~10 bestanden in `src/`
  en `sim/android/`). Let op de LVGL-ASCII-valkuil hierboven: in de *ingebouwde* tabel geen
  `€`/`·`/`✓`. Zet daarna nieuwe sleutels ook in `lang/filatrack_lang_de.conf` — de self-test
  faalt als een taal niet compleet is.
- **Drie plekken met tekst, niet één** (dit kostte twee ronden "toch niet alles Engels"):
  1. `webctl_page.h` JS → `t('sleutel','fallback')`
  2. `webctl_page.h` statische HTML → `data-i18n=` / `data-i18n-ph=` op het element
  3. **`webctl.cpp` serverantwoorden** → `send_msg(fd, status, "sleutel")`. Die tekst zet de
     pagina rechtstreeks op het scherm, dus vertalen aan de webkant helpt daar niets.
  Ook `src/ui_move.cpp` (`move_blocked`) — die reden gaat naar tablet én web.
- **Zoek nooit op Nederlandse wóórden** — dat ging vier keer mis: elke ronde miste een andere
  hoek ("materialen", "geselecteerd", "stabiel", "— kies —"). Draai `python tools/i18n_audit.py`:
  die pakt élke tekst die op het scherm kan komen, zonder woordenlijst, en jij beoordeelt de
  lijst. Hij faalt hard op een `t`-overschaduwing en op een `title=`/`placeholder=` zonder
  sleutel. Zelfs dat script had eerst een gat (het zag `o+=` maar niet `var o='<option>'`),
  dus: **vindt de audit niks en zie je het tóch op het scherm, verdenk dan eerst de audit.**
- **Valkuilen i18n**:
  - **JS die een element opnieuw opbouwt overschrijft `data-i18n`.** Zo overleefde
    "alle materialen" een "vertaalde" pagina: de dropdown wordt door JS gevuld. Bouwt JS de
    tekst? Dan moet daar `t()` staan, niet alleen `data-i18n` in de HTML.
  - `applyI18n()` doet `el.innerHTML = ...`. Zet `data-i18n` dus **nooit** op een element dat
    een control omsluit — de `<label>`s bij importeren/herstellen bevatten een
    `<input type=file>`; die zou verdwijnen. Tekst in een eigen `<span>` zetten.
  - Noem niets `t` in een scope waar je vertaalt — niet als callback-parameter
    (`function(t)`) en niet als lusvariabele (`for(var t=0;...)`). Dat overschaduwt `t()`.
    De audit-script waarschuwt hierop.
  - **Vertalingen zijn HTML** (`&hellip;`, `&amp;`, `<b>`). `innerHTML` rendert dat, maar
    `placeholder`/`textContent`/`alert()` tonen het letterlijk. Daarom houdt de pagina twee
    tabellen: `I18N` (rauw, voor data-i18n) en `I18NT` (gedecodeerd, wat `t()` teruggeeft).
  - **Gebruikersdata niet vertalen**: "gebruikt" in de rollenlijst is Arno's eigen notitie
    (`note=`), geen interface-tekst.
- **De wachtrij is asynchroon** (`q_push` → hoofdthread → `webctl_loop`). Alles wat LVGL of
  opslag aanraakt moet daar doorheen; rechtstreeks vanaf de HTTP-thread geeft een race met de
  UI die eruit leest. Gevolg: een endpoint dat meteen antwoordt, is nog **niet uitgevoerd**.
  Daarom deed `/setcfg?lang=` er een paar klikken over: de pagina haalde `/lang` op vóór de
  drain. `/setcfg` gebruikt nu `q_wait()` (~1 frame) en is dus wél waar.
  - De 6 `setTimeout(...,300)` na `/spool_save` en `/empty_save` zijn dezelfde omzeiling.
    Die werken (300 ms > 1 frame), dus laten staan. De taalwissel was de enige zonder
    vertraging — daarom brak alléén die.
  - De webserver is **één thread** die verbindingen serieel afhandelt. Elke `q_wait` blokkeert
    dus al het webverkeer zolang hij duurt. Bewust alleen op `/setcfg` gezet; zet het er niet
    zomaar overal bij.
  - **Zet nooit een `Start-Sleep` in de self-test om iets "stabiel" te krijgen** — dat is hoe
    deze bug maandenlang onzichtbaar bleef. Een test die wacht, test het wachten.
  - `send_resp()` wil een lengte; met vertaalde tekst klopt een handgeteld getal niet meer.
    Daarom `send_msg()`, die `strlen(T(key))` pakt.
  - Taalbestand: max **320 bytes** per tekst, 400 sleutels. De tablet meldt afkappen in
    `adb logcat -s SDL/APP | grep LANG` in plaats van het stil te doen.
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

X:\projects\panda\FilaTrack\tools\selftest.ps1    # 28 checks tegen de echte tablet
```

- **Git op de NAS**: de repo staat op een share, dus git wil een `safe.directory`-regel voor
  het **exacte pad**. Na de hernoeming klopte die niet meer ("dubious ownership"). Staat nu
  goed; komt hij terug na een hernoeming, dan:
  `git config --global --add safe.directory '%(prefix)///192.168.2.111/data/projects/panda/<map>'`

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
