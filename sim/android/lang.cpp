// Translations - see lang.h for the design.
#include "lang.h"
#include "storage.h"     // g_lang, save_settings()
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dirent.h>

#define LANG_FILE_FMT "/sdcard/filatrack_lang_%s.conf"

// --- built-in strings -----------------------------------------------------
// Keys are grouped by screen. English first (also the fallback), then Dutch.
// Adding a language does NOT belong here - drop a filatrack_lang_<code>.conf on
// the tablet instead. This table only exists so the app works out of the box.
struct Builtin { const char* key; const char* en; const char* nl; };
static const Builtin BUILTIN[] = {
    // --- generic ---
    {"ok",                  "OK",                     "OK"},
    {"cancel",              "Cancel",                 "Annuleren"},
    {"back",                "Back",                   "Terug"},
    {"save",                "Save",                   "Opslaan"},
    {"delete",              "Delete",                 "Verwijderen"},
    {"close",               "Close",                  "Sluiten"},
    {"refresh",             "Refresh",                "Ververs"},
    {"none",                "none",                   "geen"},
    {"unknown",             "unknown",                "onbekend"},

    // --- navigation ---
    {"nav.dashboard",       "Dashboard",              "Dashboard"},
    {"nav.files",           "Files",                  "Bestanden"},
    {"nav.move",            "Move",                   "Bewegen"},
    {"nav.spools",          "Spools",                 "Rollen"},
    {"nav.history",         "History",                "Historie"},
    {"nav.settings",        "Settings",               "Instellingen"},

    // --- dashboard ---
    {"dash.connected",      "Printer: connected",     "Printer: verbonden"},
    {"dash.offline",        "Printer: offline",       "Printer: offline"},
    {"dash.connecting",     "Connecting...",          "Verbinden..."},
    {"dash.waiting",        "Waiting for data...",    "Wachten op gegevens..."},
    {"dash.printing",       "Printing",               "Bezig met printen"},
    {"dash.paused",         "Paused",                 "Gepauzeerd"},
    {"dash.finished",       "Finished",               "Klaar"},
    {"dash.failed",         "Failed",                 "Mislukt"},
    {"dash.preparing",      "Preparing",              "Voorbereiden"},
    {"dash.idle",           "Idle",                   "Inactief"},
    {"dash.nozzle",         "Nozzle",                 "Nozzle"},
    {"dash.bed",            "Bed",                    "Bed"},
    {"dash.chamber",        "Chamber",                "Kamer"},
    {"dash.layer",          "layer",                  "laag"},
    {"dash.left",           "left",                   "resterend"},
    {"dash.eta",            "done at",                "klaar om"},
    {"dash.this_print",     "this print",             "deze print"},
    {"dash.controls",       "Controls",               "Bediening"},
    {"dash.pause",          "Pause",                  "Pauze"},
    {"dash.resume",         "Resume",                 "Hervat"},
    {"dash.stop",           "Stop",                   "Stop"},
    {"dash.light",          "Light",                  "Licht"},
    {"dash.fan",            "Fan",                    "Ventilator"},
    {"dash.brightness",     "Brightness",             "Helderheid"},
    {"dash.external_spool", "External spool:",        "Externe spoel:"},
    {"dash.filament_ams",   "Filament / AMS",         "Filament / AMS"},
    {"dash.stop_confirm_t", "Stop print?",            "Print stoppen?"},
    {"dash.stop_confirm_b", "Are you sure you want to abort this print?\nThis cannot be undone.",
                            "Weet je zeker dat je de print wilt afbreken?\nDit kan niet ongedaan worden gemaakt."},
    {"dash.humidity",       "humidity",               "vocht"},
    {"dash.hum_dry",        "dry",                    "droog"},
    {"dash.hum_ok",         "fair",                   "redelijk"},
    {"dash.hum_wet",        "damp",                   "vochtig"},
    {"dash.warn_low",       "Filament almost out",    "Filament bijna op"},
    {"dash.warn_short",     "Filament short",         "Filament tekort"},
    {"dash.in_use",         "In use now",             "Wordt nu gebruikt"},

    // --- spools ---
    {"spools.title",        "Spools",                 "Rollen"},
    {"spools.stock",        "Stock",                  "Voorraad"},
    {"spools.rolls",        "rolls",                  "rollen"},
    {"spools.filament",     "filament",               "filament"},
    {"spools.value",        "value",                  "waarde"},
    {"spools.new_roll",     "New roll",               "Nieuwe rol"},
    {"spools.edit_roll",    "Edit roll",              "Rol bewerken"},
    {"spools.empty_spools", "Empty spools",           "Lege spoelen"},
    {"spools.material",     "Material",               "Materiaal"},
    {"spools.colour",       "Colour",                 "Kleur"},
    {"spools.remaining",    "Filament left (g)",      "Resterend filament (g)"},
    {"spools.price",        "Price (EUR/kg)",         "Prijs (EUR/kg)"},
    {"spools.note",         "Note",                   "Notitie"},
    {"spools.weigh",        "Weigh",                  "Weeg"},
    {"spools.almost_empty", "almost empty",           "bijna leeg"},
    {"spools.pick_roll",    "Pick a roll",            "Kies rol"},
    {"spools.pick_for",     "Pick a roll for",        "Kies rol voor"},
    {"spools.empty_slot",   "Empty",                  "Leeg"},
    {"spools.no_roll",      "no roll in this slot",   "geen rol in dit slot"},
    {"spools.same_mat",     "Same material as this slot currently holds",
                            "Zelfde materiaal als er nu in dit slot zit"},

    // --- history / stats ---
    {"hist.title",          "History / cost",         "Historie / kosten"},
    {"hist.stats",          "Statistics",             "Statistieken"},
    {"hist.succeeded",      "Succeeded",              "Geslaagd"},
    {"hist.failed",         "Failed",                 "Mislukt"},
    {"hist.wasted",         "wasted",                 "verspild"},
    {"hist.per_print",      "Avg/print",              "Gem./print"},
    {"hist.print_time",     "Print time",             "Printtijd"},
    {"hist.timeline",       "Timeline",               "Tijdlijn"},
    {"hist.this_week",      "This week",              "Deze week"},
    {"hist.this_month",     "This month",             "Deze maand"},
    {"hist.this_year",      "This year",              "Dit jaar"},
    {"hist.per_material",   "Per material",           "Per materiaal"},
    {"hist.per_month",      "Prints per month",       "Prints per maand"},
    {"hist.search",         "Search by name...",      "Zoek op naam..."},
    {"hist.all_materials",  "all materials",          "alle materialen"},
    {"hist.newest",         "newest first",           "nieuwste eerst"},
    {"hist.no_prints",      "no prints yet",          "nog geen prints"},

    // --- files / move / scale ---
    {"files.up",              "&#11014; Up",               "&#11014; Omhoog"},
    {"move.step",             "Step:",                     "Stap:"},
    {"move.extruder",         "Extruder",                  "Extruder"},
    {"move.length",           "Length",                    "Lengte"},
    {"move.extrude",          "Extrude",                   "Extruderen"},
    {"move.retract",          "Retract",                   "Terugtrekken"},
    {"move.off",              "Off",                       "Uit"},
    {"move.motors_pos",       "Motors &amp; positions",    "Motoren &amp; posities"},
    {"move.motors_off",       "Motors off",                "Motoren uit"},
    {"move.center",           "To centre",                 "Naar midden"},
    {"move.bed_front",        "Bed forward",               "Bed naar voren"},
    {"move.z_up",             "Z up",                      "Z omhoog"},
    {"scale.back_set",        "&#8592; Settings",          "&#8592; Instellingen"},
    {"scale.calibration",     "Calibration",               "Kalibratie"},
    {"scale.tare",            "Tare (zero)",               "Tarra (nulstellen)"},
    {"scale.calibrate",       "Calibrate",                 "Kalibreer"},
    {"scale.network",         "Network",                   "Netwerk"},
    {"scale.ip_label",        "Scale IP (where the tablet looks for it)", "Schaal-IP (waar de tablet de schaal zoekt)"},
    {"scale.save_tablet",     "Save on tablet",            "Opslaan op tablet"},
    {"scale.fix_ip",          "Fixed IP on scale",         "Vast IP op schaal"},
    {"scale.wifi_label",      "Change the scale's WiFi",   "WiFi van de schaal wijzigen"},
    {"scale.wifi_name",       "WiFi name",                 "WiFi-naam"},
    {"scale.password",        "password",                  "wachtwoord"},
    {"scale.wifi_change",     "Change WiFi",               "WiFi wijzigen"},

    // --- spools / history extras ---
    {"spools.name",           "Name / brand",              "Naam / merk"},
    {"spools.empty_weight",   "Empty spool",               "Leeg spoel"},
    {"spools.new",            "New",                       "Nieuw"},
    {"spools.add",            "Add",                       "Toevoegen"},
    {"spools.weight_g",       "Weight (g)",                "Gewicht (g)"},
    {"hist.ok_and_fail",      "succeeded + failed",        "gelukt + mislukt"},
    {"hist.only_ok",          "only succeeded",            "alleen gelukt"},
    {"hist.only_fail",        "only failed",               "alleen mislukt"},
    {"hist.costliest",        "most expensive first",      "duurste eerst"},
    {"hist.most_filament",    "most filament",             "meeste filament"},
    {"hist.show_arch",        "Show archived",             "Gearchiveerd tonen"},
    {"spools.sort_name",      "name A-Z",                  "naam A-Z"},
    {"spools.sort_price",     "price/kg",                  "prijs/kg"},

    // --- generated lists / diagnostics ---
    {"of",                    "of",                        "van"},
    {"hist.no_waste",         "no waste",                  "geen verspilling"},
    {"total",                 "total",                     "totaal"},
    {"prints",                "prints",                    "prints"},
    {"hist.based_on",         "Based on your log",         "Op basis van je logboek"},
    {"hist.log_empty",        "No prints in the log yet.", "Nog geen prints in het logboek."},
    {"spools.weigh_roll",     "Weigh this roll",           "Weeg de rol"},
    {"copy",                  "Copy",                      "Kopieer"},
    {"edit",                  "Edit",                      "Bewerk"},
    {"spools.none_found",     "No rolls found.",           "Geen rollen gevonden."},
    {"diag.connected",        "connected",                 "verbonden"},
    {"diag.last_status",      "Last status",               "Laatste status"},
    {"diag.not_set",          "not set",                   "niet ingesteld"},
    {"diag.uptime",           "App running for",           "App draait al"},

    // --- forms / bulk bar / settings extras ---
    {"spools.pick_library",   "&mdash; pick from library &mdash;", "&mdash; kies uit bibliotheek &mdash;"},
    {"spools.nozzle_min",     "Nozzle min",                "Nozzle min"},
    {"spools.nozzle_max",     "Nozzle max",                "Nozzle max"},
    {"spools.bambu_code",     "Bambu code",                "Bambu-code"},
    {"spools.name_short",     "Name",                      "Naam"},
    {"spools.sort_low",       "least &rarr; most",         "weinig &rarr; veel"},
    {"spools.sort_high",      "most &rarr; least",         "veel &rarr; weinig"},
    {"spools.weight",         "Weight",                    "Gewicht"},
    {"spools.material_dots",  "material&hellip;",          "materiaal&hellip;"},
    {"spools.set",            "Set",                       "Zet"},
    {"spools.price_short",    "Price",                     "Prijs"},
    {"spools.deselect",       "Deselect",                  "Deselecteer"},
    {"spools.export_title",   "Export / import rolls",     "Rollen exporteren / importeren"},
    {"set.scale_hint",        "Weight, tare, calibration and the scale's WiFi/IP.", "Gewicht, tarra, kalibreren en WiFi/IP van de schaal."},
    {"set.manage_scale",      "Manage scale",              "Schaal beheren"},
    {"set.save_connect",      "Save &amp; connect",        "Opslaan &amp; verbinden"},
    {"set.ntfy_topic",        "ntfy topic (empty = off)",  "ntfy topic (leeg = uit)"},
    {"set.test",              "Test",                      "Test"},

    // --- live status / units ---
    {"on",                    "on",                        "aan"},
    {"off",                   "off",                       "uit"},
    {"min",                   "min",                       "min"},
    {"dash.short_body",       "short for this print on the active slot.", "te kort voor deze print op het actieve slot."},
    {"hour",                  "h",                         "uur"},
    {"days",                  "days",                      "dagen"},
    {"loading",               "loading&hellip;",           "laden&hellip;"},
    {"no_conn",               "no connection",             "geen verbinding"},
    {"empty",                 "empty",                     "leeg"},
    {"no_conn_tablet",        "no connection to the tablet", "geen verbinding met de tablet"},
    {"no_results",            "no results",                "geen resultaten"},
    {"preview",               "Preview",                   "Voorbeeld"},
    {"hist.archived",         "archived",                  "gearchiveerd"},

    // --- settings ---
    {"set.printer",         "Printer",                "Printer"},
    {"set.printer_ip",      "Printer IP",             "Printer-IP"},
    {"set.serial",          "Serial",                 "Serienummer"},
    {"set.access_code",     "Access code",            "Toegangscode"},
    {"set.notifications",   "Notifications (ntfy)",   "Meldingen (ntfy)"},
    {"set.low_threshold",   "Warn when a roll drops below (grams)",
                            "Waarschuwen als een rol onder dit aantal gram komt"},
    {"set.backup",          "Backup & restore (everything)",
                            "Back-up & herstel (alles)"},
    {"set.diagnostics",     "Diagnostics",            "Diagnose"},
    {"set.language",        "Language",               "Taal"},
    {"set.lang_hint",       "Applies to the tablet and this page. Adding a language? Drop a filatrack_lang_&lt;code&gt;.conf on the tablet - see lang/README.md.",
                            "Geldt voor de tablet en deze pagina. Een taal toevoegen? Zet een filatrack_lang_&lt;code&gt;.conf op de tablet - zie lang/README.md."},
    {"set.lang_saved",      "Language saved.",        "Taal opgeslagen."},
    {"set.scale",           "FilaTrack Scale",        "FilaTrack Scale"},
    {"set.screensaver",     "Screensaver",            "Screensaver"},
};
static const int NBUILTIN = (int)(sizeof(BUILTIN) / sizeof(BUILTIN[0]));

// --- runtime overrides (from /sdcard/filatrack_lang_<code>.conf) -----------
#define OVR_MAX 400
struct Ovr { char key[48]; char val[160]; };
static Ovr  g_ovr[OVR_MAX];
static int  g_ovr_n = 0;
static char g_active[8] = "en";
static bool g_use_nl = false;   // built-in column to read when there's no override

static void ovr_clear() { g_ovr_n = 0; }

static void ovr_load(const char* code) {
    char path[64];
    snprintf(path, sizeof(path), LANG_FILE_FMT, code);
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[224];
    while (fgets(line, sizeof(line), f) && g_ovr_n < OVR_MAX) {
        char* nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        if (!line[0] || line[0] == '#') continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        strncpy(g_ovr[g_ovr_n].key, line, sizeof(g_ovr[0].key) - 1);
        g_ovr[g_ovr_n].key[sizeof(g_ovr[0].key) - 1] = 0;
        strncpy(g_ovr[g_ovr_n].val, eq + 1, sizeof(g_ovr[0].val) - 1);
        g_ovr[g_ovr_n].val[sizeof(g_ovr[0].val) - 1] = 0;
        g_ovr_n++;
    }
    fclose(f);
    Serial.printf("LANG: %s - %d string(s) from %s\n", code, g_ovr_n, path);
}

void lang_init() {
    const char* code = g_lang[0] ? g_lang : "en";
    strncpy(g_active, code, sizeof(g_active) - 1);
    g_active[sizeof(g_active) - 1] = 0;
    g_use_nl = (strcmp(g_active, "nl") == 0);
    ovr_clear();
    ovr_load(g_active);
    Serial.printf("LANG: active = %s\n", g_active);
}

void lang_set(const char* code) {
    if (!code || !code[0]) return;
    strncpy(g_lang, code, sizeof(g_lang) - 1);
    g_lang[sizeof(g_lang) - 1] = 0;
    save_settings();
    lang_init();
}

const char* lang_current() { return g_active; }

const char* T(const char* key) {
    if (!key) return "";
    for (int i = 0; i < g_ovr_n; i++)                    // 1. language file wins
        if (!strcmp(g_ovr[i].key, key)) return g_ovr[i].val;
    for (int i = 0; i < NBUILTIN; i++)                   // 2. built-in
        if (!strcmp(BUILTIN[i].key, key)) {
            const char* s = g_use_nl ? BUILTIN[i].nl : BUILTIN[i].en;
            if (s && s[0]) return s;
            return BUILTIN[i].en;                        // 3. English fallback
        }
    return key;                                          // 4. show the key, never blank
}

int lang_codes(char out[][8], int max) {
    int n = 0;
    const char* fixed[] = {"en", "nl"};
    for (int i = 0; i < 2 && n < max; i++) { strncpy(out[n], fixed[i], 7); out[n][7] = 0; n++; }
    // plus every filatrack_lang_<code>.conf on the tablet
    DIR* d = opendir("/sdcard");
    if (!d) return n;
    struct dirent* e;
    while ((e = readdir(d)) && n < max) {
        const char* p = strstr(e->d_name, "filatrack_lang_");
        if (!p || p != e->d_name) continue;
        const char* code = e->d_name + strlen("filatrack_lang_");
        const char* dot = strstr(code, ".conf");
        if (!dot || dot == code || dot - code > 7) continue;
        char c[8]; int len = (int)(dot - code);
        memcpy(c, code, len); c[len] = 0;
        bool dup = false;
        for (int i = 0; i < n; i++) if (!strcmp(out[i], c)) { dup = true; break; }
        if (!dup) { strncpy(out[n], c, 7); out[n][7] = 0; n++; }
    }
    closedir(d);
    return n;
}

// JSON-escape into a bounded buffer.
static int jesc(const char* in, char* o, int n) {
    int j = 0;
    for (const char* s = in; *s && j < n - 2; s++) {
        if (*s == '"' || *s == '\\') { if (j < n - 3) o[j++] = '\\'; o[j++] = *s; }
        else if (*s == '\n') { if (j < n - 3) { o[j++] = '\\'; o[j++] = 'n'; } }
        else if ((unsigned char)*s < 0x20) o[j++] = ' ';
        else o[j++] = *s;
    }
    o[j] = 0;
    return j;
}

void lang_json(char* out, int n) {
    int p = 0;
    p += snprintf(out + p, n - p, "{\"lang\":\"%s\",\"s\":{", g_active);
    bool first = true;
    for (int i = 0; i < NBUILTIN && p < n - 8; i++) {
        char v[224];
        jesc(T(BUILTIN[i].key), v, sizeof(v));
        p += snprintf(out + p, n - p, "%s\"%s\":\"%s\"", first ? "" : ",", BUILTIN[i].key, v);
        first = false;
    }
    // strings that only exist in a language file (not in the built-in table)
    for (int i = 0; i < g_ovr_n && p < n - 8; i++) {
        bool known = false;
        for (int b = 0; b < NBUILTIN; b++)
            if (!strcmp(BUILTIN[b].key, g_ovr[i].key)) { known = true; break; }
        if (known) continue;
        char v[224];
        jesc(g_ovr[i].val, v, sizeof(v));
        p += snprintf(out + p, n - p, ",\"%s\":\"%s\"", g_ovr[i].key, v);
    }
    snprintf(out + p, n - p, "}}");
}
