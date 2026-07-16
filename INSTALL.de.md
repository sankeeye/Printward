# Printward installieren

[English](INSTALL.md) · [Nederlands](INSTALL.nl.md) · **Deutsch**

Zwei Teile. Die **Tablet-App** brauchst du immer; die **Waage** nur, wenn du die
Printward-Scale-Hardware hast. Keine Entwickler-Tools, kein Selberbauen.

---

## 1. Die App auf dem Tablet

**Du brauchst:** ein Android-Tablet und drei Dinge von deinem Drucker — seine **IP-Adresse**,
seine **Seriennummer** und den **LAN-Zugangscode** (alle auf dem Bildschirm des Druckers
selbst, unter Einstellungen ▸ WLAN / Netzwerk).

1. **App holen.** Öffne auf dem Tablet einen Webbrowser und gehe zur
   [**Releases-Seite**](https://github.com/sankeeye/Printward/releases). Lade **`Printward.apk`**
   herunter. *(Oder lade sie auf einem Computer herunter und kopiere sie per USB aufs Tablet.)*

2. **Installieren.** Tippe auf die heruntergeladene `Printward.apk`. Android warnt, dass es
   keine Apps aus dieser Quelle installieren darf — tippe auf **Einstellungen**, aktiviere
   **„Aus dieser Quelle zulassen"**, gehe zurück und tippe auf **Installieren**. *(Das ist bei
   jeder App normal, die nicht aus dem Play Store kommt.)*

3. **Ersteinrichtung.** Öffne Printward. Es fragt nach deinem Drucker:
   - **Drucker-IP** — z. B. `192.168.1.50`, vom Netzwerkbildschirm des Druckers oder deinem Router.
   - **Seriennummer** — auf dem Bildschirm des Druckers oder dem Aufkleber.
   - **Zugangscode** — der LAN-Zugangscode des Druckers (Einstellungen ▸ WLAN am Drucker).

   Tippe auf **Speichern & verbinden**. Du siehst sofort den Status des Druckers.

4. **Die Webseite (optional).** Du kannst den Drucker auch aus jedem Browser in deinem Netzwerk
   steuern. Adresse und Passwort stehen auf dem Tablet unter **Einstellungen ▸ Webadresse** und
   **Einstellungen ▸ Web-Passwort** (jedes Tablet erzeugt sein eigenes Passwort — melde dich als
   Benutzer `printward` an).

> **Tipp:** Gib dem Tablet eine feste IP-Adresse in deinem Router (eine „DHCP-Reservierung"),
> damit sich seine Adresse nie ändert.

---

## 2. Die Waage (optional)

Nur wenn du die **Printward Scale** hast (ESP32-S3 + Wägezelle). Sie wiegt deine Spulen, damit
die App das Filament genau verfolgt.

1. Öffne auf einem **Computer** (kein Handy) mit **Chrome** oder **Edge**:
   **<https://sankeeye.github.io/Printward/scale/>**

2. Verbinde die Waage per **USB-C**-Kabel mit dem Computer.

3. Klicke auf **Connect & Install**, wähle den Port der Waage aus der Liste und bestätige. Das
   Flashen dauert etwa eine Minute — auf deinem Computer ist nichts zu installieren.

4. **Ersteinrichtung.** Nach dem Flashen erstellt die Waage ihr eigenes WLAN-Netzwerk. Verbinde
   dich damit, gib dein Heim-WLAN ein und kalibriere dann mit einem bekannten Gewicht. Details
   in [`scale/README.md`](scale/README.md). Trage schließlich auf dem Tablet die IP der Waage
   unter **Einstellungen ▸ Printward Scale** ein.

---

## Brauche ich die Waage?

Nein. Ohne sie tippst du beim Hinzufügen einer Spule ihr Gewicht ein, und die App zählt es
während des Druckens herunter. Die Waage lässt dich nur wiegen statt tippen (und für den exakten
Wert neu wiegen). Für echte Genauigkeit ohne Waage speist das optionale
[Bambu-Cloud-Gewicht-Relay](tools/) jeden Druck mit den echten Gramm aus Bambu Cloud.
