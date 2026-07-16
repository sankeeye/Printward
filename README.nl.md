# FilaTrack voor Bambu Lab

[English](README.md) · **Nederlands** · [Deutsch](README.de.md)

Een zelfstandig **bedien‑ en monitorpaneel voor een Bambu Lab 3D‑printer** dat draait op een
**Android‑tablet** — met daarnaast een volledige webinterface, zodat je de printer kunt bedienen
vanaf de tablet of vanuit elke browser op je netwerk. Een losse **FilaTrack Scale** (weegschaal)
voegt echte spoelgewicht‑tracking toe. Zie [Hardware](#hardware).

De interface is beschikbaar in **Engels, Nederlands en Duits**, direct wisselbaar, en er kunnen
meer talen bij door simpelweg een bestand neer te zetten — zie [`lang/`](lang/).

## Functies

- **Live printerstatus & bediening** via de **lokale MQTT** van de printer (de broker in de
  printer zelf, bereikbaar op je netwerk met de LAN‑toegangscode — werkt ook in de gewone
  cloud‑modus van de printer, geen *"LAN Only"*‑modus nodig): printvoortgang, temperaturen en
  pauze/hervat/stop/licht‑bediening.
- **Filament‑ / AMS‑ & spoelgewicht‑tracking**, inclusief automatisch aftrekken van de grammen
  die elke voltooide print heeft gebruikt (zie de gewicht‑relay hieronder).
- **Bestandsbrowser** via de **FTP**‑dienst van de printer.
- **Bambu Cloud‑integratie** voor het enige dat de lokale MQTT nooit prijsgeeft: hoeveel gram
  een voltooide print daadwerkelijk heeft verbruikt.
- **LVGL touch‑UI** (LVGL 9.3): printerscherm, filament/AMS, bestandsbrowser, instellingen.
- **Setup op het scherm** voor de printerverbinding (IP / serienummer / LAN‑toegangscode).

Op de **Android‑tablet / web‑build**, aanvullend:

- **Handmatig bewegen** ("Move"): jog X/Y/Z, homen, extruderen/terugtrekken en — in LAN‑modus —
  temperatuur‑presets.
- **Volledige webbediening** op `:8080` die elk tabletscherm nabootst (dashboard, filament,
  bestanden, bewegen, weegschaal, rollen, instellingen), **beveiligd met een wachtwoord en
  alleen bereikbaar op je eigen netwerk**.
- **Weegschaal‑gebaseerde filamentbeheerder**: een rollen‑bibliotheek (aanmaken/bewerken/
  kopiëren/zoeken/bulk‑bewerken), rollen wegen met de **FilaTrack Scale**, live resterende
  grammen die tijdens een print aftellen, prijs / kosten / restwaarde, en een waarschuwing
  *"komt deze print filament tekort?"*.
- **ntfy‑meldingen** (print klaar/mislukt, filament tekort), **printhistorie & statistieken**,
  en kiosk‑herstart na een crash.

> **Let op — temperatuur & prints starten.** Op recente Bambu‑firmware (≥ 01.08) weigert de
> printer *temperatuur instellen* en *print starten* van externe tools, tenzij hij in **LAN
> Only**‑modus staat. FilaTrack heeft een **LAN‑modus**‑schakelaar (Instellingen ▸ Printer
> instellen): zet die aan als je printer op LAN Only staat, en de temperatuurknoppen en het
> starten van prints verschijnen. In de gewone cloud‑modus laat je hem uit en start je prints
> via Bambu Studio / Handy — al het andere hier (monitoren, pauze/stop, licht, bewegen/jog,
> AMS‑instellingen) werkt sowieso.

## Hardware

- **Android‑tablet** — draait de LVGL‑UI (`src/ui_*`) als zelfstandige printer‑monitor/bediening
  met een live lokale‑MQTT‑verbinding naar de printer, **plus een volledige web‑UI** op
  `http://<tablet>:8080`. Geverifieerd op een Samsung SM‑T280 (Android 5.1.1).
  Zie [`android/README.md`](android/README.md).
- **FilaTrack Scale** — een ESP32‑S3 + HX711 loadcell‑weegschaal (de SpoolEase Scale‑hardware,
  geflasht met onze eigen firmware) die echte spoelgewichten naar de tablet stuurt voor
  filament‑tracking, kosten en filament‑waarschuwingen. Zie [`scale/`](scale/).

Ontwikkeld en getest op een **Bambu Lab P1S**. FilaTrack praat met de printer via de lokale
**MQTT + FTP**‑interface die Bambu Lab‑printers delen, dus andere modellen (P1P, X1C, A1, …)
zouden ook moeten werken — die zijn hier alleen nog niet getest.

Exact dezelfde `src/ui_*`‑code bouwt ook als **PC‑simulator** (`sim/`) voor ontwikkeling.

## Bouwen & flashen

- **Android‑tablet** — bouw de APK met de Android NDK + Gradle en zet hem op de tablet. Volledige
  stap‑voor‑stap in [`android/README.md`](android/README.md). Het printer‑IP / serienummer /
  de LAN‑toegangscode komen in `/sdcard/filatrack.conf` (zie `sim/android/filatrack.conf.example`)
  of via de **Printer instellen** op het scherm.
- **FilaTrack Scale** — open `scale/` in **VS Code** met de **PlatformIO**‑extensie en flash via
  **USB‑C** (omgeving `filatrack_scale`). WiFi‑setup bij de eerste start en het kalibreren van de
  loadcell staan beschreven in `scale/`.

## Beveiliging

De webinterface bedient een echte printer, dus hij staat niet open:

- **Wachtwoord.** De tablet genereert bij de eerste start zelf een willekeurig wachtwoord en
  toont het onder **Instellingen ▸ Webwachtwoord** — zoals een tv een koppelcode toont, dus er is
  geen standaardwachtwoord dat iemand vergeet te wijzigen en elk apparaat krijgt zijn eigen.
  Log in als gebruiker `filatrack`.
- **Alleen lokaal netwerk.** De server weigert verbindingen van buiten je eigen netwerk, zodat een
  doorgezette poort of UPnP de printer niet op het internet kan zetten.
- De LAN‑toegangscode van de printer blijft op de tablet (in `/sdcard/filatrack.conf`) en zit
  nooit in een back‑up of in de webinterface.

Het is gewoon HTTP, dus het wachtwoord gaat onversleuteld over je eigen netwerk — genoeg om de
printer van het internet en van andere apparaten op je wifi te houden, geen bescherming tegen
iemand die actief je netwerkverkeer afluistert.

## Filament‑gewicht‑relay (`tools/`)

De login van Bambu Cloud zit achter Cloudflare‑botbescherming, dus draait er in plaats daarvan
een klein Python‑hulpje op je pc: het logt in bij Bambu Cloud, volgt je printhistorie en meldt
de grammen die elke voltooide print gebruikte aan de tablet, over je netwerk.

Instellen:

1. `pip install requests`
2. Kopieer `tools/config.example.json` naar `tools/config.json` en vul je gegevens in
   (Bambu‑e‑mail, printer‑serienummer, het IP, de poort en het webwachtwoord van de tablet - Instellingen > Webwachtwoord). `config.json` en `relay_state.json`
   staan in git‑ignore — ze bevatten je inloggegevens/token en moeten lokaal blijven.
3. Start het: `python tools/bambu_weight_relay.py` — laat het draaien tijdens het printen.

Opmerkingen:

- **Verificatiecode**: ongeveer elke 90 dagen vraagt een nieuwe login om een 6‑cijferige code per
  e‑mail; de relay vraagt die in zijn terminal, dus draai hem met een venster bij de eerste login.
- **Windows‑gemak**: `encrypt_password.bat` bewaart je wachtwoord versleuteld (`bambu_password_enc`,
  via Windows DPAPI gekoppeld aan je account) in plaats van platte tekst; `start_relay_hidden.vbs`
  draait de relay zonder consolevenster. Deze zijn optioneel en alleen voor Windows — op Linux/macOS
  gebruik je gewoon een `bambu_password` in platte tekst in je (git‑ignored) `config.json`, of een
  `custom_token`.

## Hoe het ontstond

FilaTrack begon als firmware voor een klein ESP32‑handpaneeltje — een plek om te zien wat de
printer aan het doen was zonder Bambu Studio op een laptop te openen. Het werkte, maar de hardware
bleek uiteindelijk het beperkende ding, en een oude Android‑tablet deed hetzelfde werk beter: een
groter scherm, wifi die de OS al regelt, en ruimte om te groeien.

En groeien deed het — en het interessante was niet nóg een statusweergave. Het was de vraag die in
de werkplaats echt opkomt: *hoeveel filament zit er nog op die spoel, en wat heeft deze print me
gekost?* Dat eerlijk beantwoorden vraagt echte gewichten, geen percentagegok, dus kwam er een tweede
apparaat bij: een ESP32‑S3 met een loadcell — de FilaTrack Scale. Daaruit kwamen de rollen‑
bibliotheek, kosten per print, de historie en de statistieken.

De ESP32‑firmware is inmiddels verwijderd. Wat overblijft is de tabletapp, de webinterface en de
weegschaal.

## Met dank aan

- Gebouwd op LVGL en SDL, plus de open‑source bibliotheken in `platformio.ini`.

## Licentie

MIT — zie [LICENSE](LICENSE). Het oorspronkelijke project is ook MIT; deze fork houdt dat aan.
