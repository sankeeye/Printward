# Installing FilaTrack

**English** · [Nederlands](INSTALL.nl.md) · [Deutsch](INSTALL.de.md)

Two parts. You always need the **tablet app**; the **scale** only if you have the
FilaTrack Scale hardware. No developer tools, no building.

---

## 1. The app on the tablet

**You need:** an Android tablet, and three things from your printer — its **IP
address**, **serial number**, and **LAN access code** (all on the printer's own
screen, under Settings ▸ WLAN / Network).

1. **Get the app.** On the tablet, open a web browser and go to the
   [**Releases page**](https://github.com/sankeeye/FilaTrack/releases). Download
   **`FilaTrack.apk`**. *(Or download it on a computer and copy it to the tablet
   over USB.)*

2. **Install it.** Tap the downloaded `FilaTrack.apk`. Android will warn that it
   can't install apps from this source — tap **Settings**, switch on
   **"Allow from this source"**, go back, and tap **Install**. *(This is normal for
   any app not from the Play Store.)*

3. **First-time setup.** Open FilaTrack. It asks for your printer:
   - **Printer IP** — e.g. `192.168.1.50`, from the printer's network screen or your router.
   - **Serial** — on the printer's screen, or its sticker.
   - **Access code** — the printer's LAN access code (Settings ▸ WLAN on the printer).

   Tap **Save & connect**. You'll see the printer's status straight away.

4. **The web page (optional).** You can also control the printer from any browser on
   your network. The address and password are on the tablet under
   **Settings ▸ Web page** and **Settings ▸ Web password** (each tablet makes its own
   password — sign in as user `filatrack`).

> **Tip:** give the tablet a fixed IP in your router (a "DHCP reservation") so its
> address never changes.

---

## 2. The scale (optional)

Only if you have the **FilaTrack Scale** (ESP32-S3 + load cell). It weighs your spools
so the app tracks filament exactly.

1. On a **computer** (not a phone) using **Chrome** or **Edge**, open:
   **<https://sankeeye.github.io/FilaTrack/scale/>**

2. Connect the scale to the computer with a **USB-C** cable.

3. Click **Connect & Install**, pick the scale's port from the list, and confirm.
   Flashing takes about a minute — nothing to install on your computer.

4. **First-time setup.** After flashing, the scale makes its own Wi-Fi network. Connect
   to it, enter your home Wi-Fi, then calibrate with a known weight. Details in
   [`scale/README.md`](scale/README.md). Finally, on the tablet, put the scale's IP
   under **Settings ▸ FilaTrack Scale**.

---

## Do I need the scale?

No. Without it, type a spool's weight when you add it and the app counts it down as you
print. The scale just lets you weigh instead of type (and re-weigh for the exact
figure). For real accuracy without a scale, the optional
[Bambu Cloud weight relay](tools/) feeds each print's real grams from Bambu Cloud.
