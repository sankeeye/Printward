# PandaScale — firmware voor de weegschaal

Onze eigen, simpele firmware voor de **SpoolEase Scale**-hardware (ESP32-S3 +
HX711 load-cell). Flash dit **in plaats van** de SpoolEase-firmware. De schaal
wordt een "domme sensor": hij meet het gewicht en geeft dat via LAN-HTTP door
aan de **PandaTouch-tablet** (die het brein is). Geen NFC, geen database, geen
printer-MQTT — alleen een gekalibreerde schaal met een mini web-API.

## Hardware / bedrading

Zoals in de SpoolEase Scale build-gids:

| HX711 | ESP32-S3 |
|-------|----------|
| DT    | GPIO 5   |
| SCK   | GPIO 4   |
| VCC   | 3V3      |
| GND   | GND      |

> Is jouw board geen standaard ESP32-S3-DevKitC, pas dan `board` in
> `platformio.ini` aan — de GPIO-nummers (4/5) blijven gelijk.

De optionele PN532 NFC-lezer en de RGB-LED (GPIO48) worden (nog) niet gebruikt.

## Flashen

```
cd scale
pio run -t upload          # ESP32-S3 via USB
pio device monitor         # toont het IP-adres na verbinden
```

## Eerste keer: WiFi instellen

Er staan **geen** WiFi-gegevens in de code. Bij de eerste start opent de schaal
een WiFi-netwerk **`PandaScale-setup`** — verbind ermee met je telefoon, kies je
eigen WiFi en vul het wachtwoord in. Daarna verbindt de schaal automatisch.

## Kalibreren (één keer)

1. Open in de browser **`http://pandascale.local/`** (of `http://<ip>/`, het IP
   staat in de seriële monitor).
2. Niets op de schaal → tik **Tarra (nulstellen)**.
3. Zet een **bekend gewicht** erop (bijv. 500 g), vul dat aantal gram in en tik
   **Kalibreer**.
4. Klaar — tarra en kalibratie blijven bewaard (NVS).

## Web-API (voor de tablet)

| Endpoint | Doet |
|----------|------|
| `GET /weight` | `{"g":852.0,"raw":123456,"stable":true}` |
| `GET /tare` | schaal op nul zetten |
| `GET /cal?known=<gram>` | kalibreren met een bekend gewicht |
| `GET /` | kalibratie-/statuspagina |

De PandaTouch-tablet pollt `GET /weight` tijdens de weeg-flow. Geef de schaal
bij voorkeur een **vast IP** in je router, dan blijft de koppeling stabiel.
