# Translating FilaTrack

FilaTrack ships with **English** and **Dutch** built in. Any other language is
just a text file — no rebuild, no toolchain, nothing to compile.

## Add a language in three steps

1. Copy `filatrack_lang_de.conf` (the German example) to
   `filatrack_lang_<code>.conf`, where `<code>` is a short language code
   (`fr`, `es`, `it`, `pl`, …).
2. Translate the lines you know. Format is one `key=text` per line; `#` starts a
   comment.
3. Push it to the tablet:

   ```
   adb push filatrack_lang_fr.conf /sdcard/filatrack_lang_fr.conf
   ```

The language appears in **Settings → Language** straight away, on the tablet
*and* on the web page — the same file feeds both, so every string is translated
only once.

## You don't have to finish it

Any key you leave out falls back to English, so a half-translated file is
perfectly usable. Translate ten lines, use it, add more when you feel like it.

## Fixing a word

The same trick works on the built-in languages. Don't like a Dutch label? Put
just that one line in `filatrack_lang_nl.conf` and it overrides the built-in
text:

```
dash.this_print=deze druk
```

## Which keys are there?

Ask the tablet — it lists every key with its current text:

```
curl http://<tablet-ip>:8080/lang
```

## Limits

One line per key, and the text after `=` has to stay under **320 bytes**
(accented letters count double: `ü` is 2). Keys stay under 48 characters, and a
file holds up to 400 of them. Go over and the tablet says so in the log rather
than quietly cutting your text off:

```
adb logcat -s SDL/APP | grep LANG
```

Watch the width too: the tablet is 480 px wide, so a label that runs much
longer than the English one gets clipped on screen even though it fits here.

Keys are grouped by screen: `dash.*` (dashboard), `nav.*` (tabs), `spools.*`,
`hist.*` (history/statistics), `set.*` (settings), plus a few generic ones
(`ok`, `cancel`, `back`, …).

## A note on the tablet's font

The tablet's built-in font covers Latin characters (including à/é/ö/ß). Scripts
outside that range — Greek, Cyrillic, Chinese, … — will show as blank boxes on
the **tablet**, though they render fine on the **web page**. Those need a font
change in the firmware; open an issue if you'd like one.
