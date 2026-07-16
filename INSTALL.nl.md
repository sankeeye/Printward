# FilaTrack installeren

[English](INSTALL.md) · **Nederlands** · [Deutsch](INSTALL.de.md)

Twee onderdelen. De **tablet-app** heb je altijd nodig; de **weegschaal** alleen als je de
FilaTrack Scale-hardware hebt. Geen ontwikkeltools, niets zelf bouwen.

---

## 1. De app op de tablet

**Je hebt nodig:** een Android-tablet, en drie dingen van je printer — het **IP-adres**, het
**serienummer** en de **LAN-toegangscode** (allemaal op het scherm van de printer zelf, onder
Instellingen ▸ WLAN / Netwerk).

1. **Haal de app op.** Open op de tablet een webbrowser en ga naar de
   [**Releases-pagina**](https://github.com/sankeeye/FilaTrack/releases). Download
   **`FilaTrack.apk`**. *(Of download hem op een computer en zet hem via USB op de tablet.)*

2. **Installeer hem.** Tik op de gedownloade `FilaTrack.apk`. Android waarschuwt dat het geen
   apps uit deze bron mag installeren — tik op **Instellingen**, zet **"Toestaan uit deze
   bron"** aan, ga terug en tik op **Installeren**. *(Dit is normaal voor elke app die niet uit
   de Play Store komt.)*

3. **Eerste keer instellen.** Open FilaTrack. Hij vraagt om je printer:
   - **Printer-IP** — bijv. `192.168.1.50`, van het netwerkscherm van de printer of je router.
   - **Serienummer** — op het scherm van de printer, of op de sticker.
   - **Toegangscode** — de LAN-toegangscode van de printer (Instellingen ▸ WLAN op de printer).

   Tik op **Opslaan & verbinden**. Je ziet meteen de status van de printer.

4. **De webpagina (optioneel).** Je kunt de printer ook bedienen vanuit elke browser op je
   netwerk. Het adres en het wachtwoord staan op de tablet onder **Instellingen ▸ Webadres** en
   **Instellingen ▸ Webwachtwoord** (elke tablet maakt zijn eigen wachtwoord — log in als
   gebruiker `filatrack`).

> **Tip:** geef de tablet een vast IP-adres in je router (een "DHCP-reservering") zodat zijn
> adres nooit verandert.

---

## 2. De weegschaal (optioneel)

Alleen als je de **FilaTrack Scale** hebt (ESP32-S3 + weegcel). Die weegt je spoelen zodat de
app het filament exact bijhoudt.

1. Open op een **computer** (geen telefoon) met **Chrome** of **Edge**:
   **<https://sankeeye.github.io/FilaTrack/scale/>**

2. Verbind de weegschaal met de computer via een **USB-C**-kabel.

3. Klik op **Connect & Install**, kies de poort van de weegschaal uit de lijst en bevestig.
   Het flashen duurt ongeveer een minuut — je hoeft niets op je computer te installeren.

4. **Eerste keer instellen.** Na het flashen maakt de weegschaal zijn eigen wifi-netwerk.
   Verbind ermee, voer je thuis-wifi in en kalibreer daarna met een bekend gewicht. Details in
   [`scale/README.md`](scale/README.md). Zet ten slotte op de tablet het IP van de weegschaal
   onder **Instellingen ▸ FilaTrack Scale**.

---

## Heb ik de weegschaal nodig?

Nee. Zonder weegschaal typ je het gewicht van een spoel in als je hem toevoegt, en telt de app
het tijdens het printen af. De weegschaal laat je alleen wegen in plaats van typen (en opnieuw
wegen voor het exacte getal). Voor echte nauwkeurigheid zonder weegschaal voedt de optionele
[Bambu Cloud-gewichtsrelay](tools/) elke print met de echte grammen uit Bambu Cloud.
