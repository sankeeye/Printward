#pragma once
#ifndef LANG_H
#define LANG_H

// Translations.
//
// Every visible string has a key ("dash.printing"). English and Dutch are
// compiled in, so the app is always usable even with no files present. On top of
// that, a plain language file dropped on the tablet
//
//     /sdcard/printward_lang_<code>.conf      key=translated text
//
// can override any built-in string OR add a whole new language - no rebuild, no
// toolchain. That's the point: someone who wants Printward in German only has to
// write a text file.
//
// The web page is served the same table via /lang, so each string is translated
// once and both the tablet and the browser use it.

// Selected language code, persisted as lang= in /sdcard/printward.conf.
// Defined in android_glue.cpp next to the other settings.
extern char g_lang[8];

// Load the language named in the config (g_lang) plus any override file.
// Call once at startup, after load_settings().
void lang_init();

// Translated text for `key`. Never returns null: falls back to English, and
// finally to the key itself, so a missing translation shows up as an obvious
// key instead of an empty screen.
const char* T(const char* key);

// Active language code, e.g. "nl".
const char* lang_current();

// Switch language: reloads the table and persists the choice. `code` is a short
// code like "en" / "nl" / "de".
void lang_set(const char* code);

// Available languages: the built-in ones plus every printward_lang_*.conf found
// on the tablet. Returns how many were written into `out`.
int lang_codes(char out[][8], int max);

// The active language as JSON ({"key":"text",...}) for the web page.
void lang_json(char* out, int n);

#endif
