// Translations - see lang.h for the design.
#include "lang.h"
#include "storage.h"     // g_lang, save_settings()
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dirent.h>

#define LANG_FILE_FMT "/sdcard/printward_lang_%s.conf"

// --- built-in strings -----------------------------------------------------
// Keys are grouped by screen. English first (also the fallback), then Dutch.
// Adding a language does NOT belong here - drop a printward_lang_<code>.conf on
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
    {"dash.plate",          "Plate",                  "Plaat"},
    {"dash.plate_auto",     "Auto",                   "Automatisch"},
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
    {"spools.number",       "Roll number #",          "Rolnummer #"},
    {"spools.number_hint",  "Unique number for this roll - write it on the spool. Filled in automatically.", "Uniek nummer voor deze rol - schrijf het op de spoel. Wordt automatisch ingevuld."},
    {"spools.note",         "Note",                   "Notitie"},
    {"spools.weigh",        "Weigh",                  "Weeg"},
    {"spools.almost_empty", "almost empty",           "bijna leeg"},
    {"spools.pick_roll",    "Pick a roll",            "Kies rol"},
    {"spools.pick_for",     "Pick a roll for",        "Kies rol voor"},
    {"spools.by_number",    "Roll number:",           "Rolnummer:"},
    {"spools.pick_it",      "Pick",                   "Pak"},
    {"spools.no_num",       "no roll #",              "geen rol #"},
    {"spools.all_mat",      "All",                    "Alle"},
    {"spools.in_ams",       "in AMS",                 "in AMS"},
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

    // --- move: what an action did (tablet hint + web reply) ---
    {"move.homing",           "Homing...", "Homing..."},
    {"move.extruding",        "Extruding...", "Extruderen..."},
    {"move.retracting",       "Retracting...", "Terugtrekken..."},
    {"move.nozzle_off",       "Nozzle off", "Nozzle uit"},
    {"move.bed_heating",      "Bed heating...", "Bed opwarmen..."},
    {"move.bed_off",          "Bed off", "Bed uit"},
    {"move.nozzle_set",       "Nozzle set", "Nozzle ingesteld"},
    {"move.bed_set",          "Bed set", "Bed ingesteld"},
    {"move.preheat_pla",      "PLA preheat", "PLA voorverwarmen"},
    {"move.preheat_petg",     "PETG preheat", "PETG voorverwarmen"},
    {"move.preheat_abs",      "ABS preheat", "ABS voorverwarmen"},
    {"move.preheat_tpu",      "TPU preheat", "TPU voorverwarmen"},
    {"move.fan_set",          "Fan set", "Fan ingesteld"},
    {"move.homing_x",         "Home X...", "Home X..."},
    {"move.homing_y",         "Home Y...", "Home Y..."},
    {"move.homing_z",         "Home Z...", "Home Z..."},
    {"move.preheating",    "Heating to 220\xC2\xB0" "C...", "Opwarmen naar 220\xC2\xB0" "C..."},

    // --- tablet screens ---
    {"set.lan_mode",          "LAN mode", "LAN-modus"},
    {"set.lan_hint",          "Turn on only if your printer is set to LAN-only mode. Adds temperature controls and lets you start prints - the printer ignores both in cloud mode.", "Zet aan als je printer op LAN-only staat. Voegt temperatuurknoppen toe en laat je prints starten - in cloud-modus negeert de printer allebei."},
    {"move.temp_section",     "Temperature", "Temperatuur"},
    {"move.fan_off",          "Fan off", "Fan uit"},

    {"set.web_port",          "Web port", "Webpoort"},
    {"set.port_range",        "Port must be between 1024 and 65535.", "Poort moet tussen 1024 en 65535 liggen."},
    {"set.port_restart",      "Saved. Restart the app for the new port.", "Opgeslagen. Herstart de app voor de nieuwe poort."},

    {"set.web_hint",        "Web page: %s - sign in with user \"printward\" and the password above. Only reachable from your own network.",
                            "Webpagina: %s - log in met gebruiker \"printward\" en het wachtwoord hierboven. Alleen bereikbaar vanaf je eigen netwerk."},
    {"set.web_pass",          "Web password", "Webwachtwoord"},

    {"dash.left_fmt",       "%dh%02dm left",             "nog %du%02dm"},

    {"ss.temps",              "Nozzle %.0f\xC2\xB0   Bed %.0f\xC2\xB0   Chamber %.0f\xC2\xB0", "Nozzle %.0f\xC2\xB0   Bed %.0f\xC2\xB0   Chamber %.0f\xC2\xB0"},
    {"ss.temps_nc",           "Nozzle %.0f\xC2\xB0   Bed %.0f\xC2\xB0", "Nozzle %.0f\xC2\xB0   Bed %.0f\xC2\xB0"},
    {"ss.short_fmt",          "Filament short: ~%.0f g", "Filament tekort: ~%.0f g te kort"},
    {"scale.net_fmt",         "network: %s \xE2\x80\xA2 %s \xE2\x80\xA2 %ddBm%s", "netwerk: %s \xE2\x80\xA2 %s \xE2\x80\xA2 %ddBm%s"},
    {"scale.static_suffix",   " \xE2\x80\xA2 static IP", " \xE2\x80\xA2 vast IP"},
    {"move.nozzle_fmt",       "Nozzle %.0f/%.0f\xC2\xB0" "C", "Nozzle %.0f/%.0f\xC2\xB0" "C"},
    {"files.print_blocked",   "Printing goes through Bambu Studio/Handy (the firmware blocks third-party start).", "Printen gaat via Bambu Studio/Handy (firmware blokkeert start van derden)."},
    {"set.printer_hint_file", "To change printer settings, edit /sdcard/printward.conf and restart the app", "Printerinstellingen wijzigen? Pas /sdcard/printward.conf aan en herstart de app"},
    {"set.printer_hint_web",  "To change printer settings, open http://%s/ in a browser", "Printerinstellingen wijzigen? Open http://%s/ in een browser"},

    {"ss.layer",              "Layer %d / %d", "Laag %d / %d"},
    {"ss.left",               "%dh %02dm left", "Nog %du %02dm"},
    {"ss.ams_none",           "AMS slot: -", "AMS-slot: -"},
    {"ss.ams_slot",           "AMS %d - slot %d", "AMS %d - slot %d"},
    {"ss.loading_model",      "Loading model...", "Model laden..."},
    {"spools.weighed_fmt",    "'%s' weighed: %.0f g left", "'%s' gewogen: %.0f g resterend"},
    {"spools.none_tap_new",   "No rolls yet - tap 'New roll'.", "Nog geen rollen - tik 'Nieuwe rol'."},
    {"spools.empty_pick",     "Empty spool (pick or grams)", "Leeg spoel (kies of gram)"},
    {"saving",                "Saving...", "Opslaan..."},
    {"set.printer_setup",     "Printer setup", "Printer instellen"},
    {"scale.need_ssid",       "enter a WiFi name", "geef een WiFi-naam"},
    {"scale.ip_saved",        "Scale IP saved", "Scale-IP opgeslagen"},
    {"nav.scale",             "Scale", "Weegschaal"},
    {"scale.connecting",      "connecting to scale…", "verbinden met schaal…"},
    {"scale.net_wait",        "network: …", "netwerk: …"},
    {"spools.pick_ext",       "Pick a roll for the external spool", "Kies rol voor de externe spoel"},
    {"spools.pick_ams",       "Pick a roll for AMS%d T%d", "Kies rol voor AMS%d T%d"},
    {"spools.empty_no_roll",  "Empty - no roll in this slot", "Leeg - geen rol in dit slot"},
    {"files.ams_on",          "Use AMS: On", "AMS gebruiken: aan"},
    {"files.ams_off_ext",     "Use AMS: Off (external spool)", "AMS gebruiken: uit (externe spoel)"},
    {"files.ams_off",         "Use AMS: Off", "AMS gebruiken: uit"},
    {"files.page",            "Page %d/%d - %d item(s)", "Pagina %d/%d - %d item(s)"},
    {"files.prev",            "< Prev", "< Vorige"},
    {"files.next",            "Next >", "Volgende >"},
    {"files.color",           "Colour %d:", "Kleur %d:"},
    {"files.skip",            "Skip", "Overslaan"},
    {"files.print",           "Print", "Printen"},
    {"move.controls",         "Controls", "Regelen"},
    {"move.extrude_10",       "Extrude 10mm", "Extruderen 10mm"},
    {"move.retract_10",       "Retract 10mm", "Terugtrekken 10mm"},
    {"dash.stop_yes",         "Stop", "Stoppen"},
    {"dash.humidity_fmt",     "Humidity: %s", "Vocht: %s"},
    {"dash.humidity_pct",     "Humidity: %d%%", "Vocht: %d%%"},
    {"dash.light_on",         "Light: On", "Licht: aan"},
    {"dash.light_off",        "Light: Off", "Licht: uit"},
    {"dash.fan_pct",          "Fan %d%%", "Ventilator %d%%"},
    {"dash.short_fmt",        "Filament short: ~%.0f g", "Filament tekort: ~%.0f g"},
    {"set.saver_3d",          "Screensaver: 3D", "Screensaver: 3D"},
    {"set.saver_2d",          "Screensaver: 2D", "Screensaver: 2D"},
    {"set.saver_delay",       "Screensaver",     "Screensaver"},
    {"set.saver_off",         "off",             "uit"},
    {"set.saver_title",       "Screensaver delay", "Screensaver-vertraging"},
    {"set.saver_hint",        "Idle seconds before the screensaver appears (0 = never)", "Aantal seconden inactief voordat de screensaver verschijnt (0 = nooit)"},
    {"set.wifi_setup",        "WiFi setup", "WiFi instellen"},
    {"wifi.selected",         "Selected: %s", "Gekozen: %s"},
    {"wifi.selected_none",    "Selected: none", "Gekozen: geen"},
    {"wifi.scanning",         "Scanning...", "Zoeken..."},
    {"wifi.pick_first",       "Pick a network above first", "Kies eerst een netwerk hierboven"},
    {"wifi.scan",             "Scan for WiFi", "Zoek WiFi"},
    {"wifi.connect_save",     "Connect & save", "Verbinden & opslaan"},
    {"wifi.connected",        "Connected! IP %s", "Verbonden! IP %s"},
    {"wifi.failed",           "Couldn't connect - check the password and try again", "Verbinden mislukt - controleer het wachtwoord"},
    {"wifi.scan_failed",      "Scan failed, try again", "Zoeken mislukt, probeer opnieuw"},
    {"wifi.none_found",       "No networks found", "Geen netwerken gevonden"},

    // --- files / move / scale ---
    {"files.up",              "&#11014; Up",               "&#11014; Omhoog"},
    {"move.step",             "Step:",                     "Stap:"},
    {"move.extruder",         "Extruder",                  "Extruder"},
    {"move.length",           "Length",                    "Lengte"},
    {"move.extrude",          "Extrude",                   "Extruderen"},
    {"move.retract",          "Retract",                   "Terugtrekken"},
    {"move.off",              "Off",                       "Uit"},
    {"move.motors_pos",       "Motors & positions",    "Motoren & posities"},
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
    {"spools.sort_num",       "by number (oldest first)",  "op nummer (oudste eerst)"},
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
    {"spools.material_dots",  "material…",          "materiaal…"},
    {"spools.set",            "Set",                       "Zet"},
    {"spools.price_short",    "Price",                     "Prijs"},
    {"spools.deselect",       "Deselect",                  "Deselecteer"},
    {"spools.export_title",   "Export / import rolls",     "Rollen exporteren / importeren"},
    {"set.scale_hint",        "Weight, tare, calibration and the scale's WiFi/IP.", "Gewicht, tarra, kalibreren en WiFi/IP van de schaal."},
    {"set.manage_scale",      "Manage scale",              "Schaal beheren"},
    {"set.save_connect",      "Save & connect",        "Opslaan & verbinden"},
    {"set.ntfy_topic",        "ntfy topic (empty = off)",  "ntfy topic (leeg = uit)"},
    {"set.test",              "Test",                      "Test"},
    {"set.dry_title",         "Dry the silica gel",        "Silicagel drogen"},
    {"set.dry_hint",          "Get a reminder when the desiccant needs drying again. 0 = off. An ntfy topic above also sends a push.", "Krijg een melding wanneer het droogmiddel weer gedroogd moet worden. 0 = uit. Een ntfy-topic hierboven geeft ook een pushmelding."},
    {"set.dry_interval",      "Dry every how many days (0 = off)", "Om de hoeveel dagen drogen (0 = uit)"},
    {"set.dry_done",          "Dried it now",              "Nu gedroogd"},
    {"set.dry_off_status",    "Off - no reminder.",        "Uit - geen herinnering."},
    {"set.dry_next",          "Next drying in",            "Volgende keer drogen over"},
    {"set.dry_over",          "overdue - dry it!",         "te laat - drogen!"},
    {"set.dry_saved",         "set to",                    "ingesteld op"},
    {"set.dry_off",           "turned off",                "uitgezet"},
    {"set.dry_reset",         "timer restarted - thanks!", "timer opnieuw gestart - bedankt!"},
    {"dash.dry_warn",         "Dry the silica gel",        "Silicagel drogen"},
    {"dash.dry_late",         "late",                      "te laat"},
    {"dash.dry_in",           "in",                        "nog"},
    {"dash.dry_today",        "today",                     "vandaag"},
    {"set.dry_advance",       "Show the banner this many days early", "Banner alvast tonen (dagen van tevoren)"},
    {"set.dry_advance_hint",  "0 = only once overdue",     "0 = pas als het over tijd is"},
    {"set.dry_always",        "Always show the banner on the tablet", "Banner altijd op de tab tonen"},
    {"set.dry_always_hint",   "Off = only within the window above", "Uit = pas binnen bovenstaand venster"},
    {"set.dry_hum",           "Also warn on AMS humidity",  "Ook waarschuwen bij AMS-vochtigheid"},
    {"set.dry_hum_hint",      "Alerts once the AMS stays damp ~30 min", "Melding zodra de AMS ~30 min vochtig meldt"},
    {"set.dry_saved2",        "saved",                      "opgeslagen"},
    {"dash.dry_wet",          "AMS damp",                   "AMS vochtig"},
    {"weeks",                 "wk",                        "wk"},

    // --- live status / units ---
    {"on",                    "on",                        "aan"},
    {"off",                   "off",                       "uit"},
    {"min",                   "min",                       "min"},
    {"dash.short_body",       "short for this print on the active slot.", "te kort voor deze print op het actieve slot."},
    {"hour",                  "h",                         "uur"},
    {"days",                  "days",                      "dagen"},
    {"loading",               "loading…",           "laden…"},
    {"no_conn",               "no connection",             "geen verbinding"},
    {"empty",                 "empty",                     "leeg"},
    {"no_conn_tablet",        "no connection to the tablet", "geen verbinding met de tablet"},
    {"no_results",            "no results",                "geen resultaten"},
    {"preview",               "Preview",                   "Voorbeeld"},
    {"hist.archived",         "archived",                  "gearchiveerd"},

    // --- static page text (hints, placeholders, buttons) ---
    {"auto",                "auto",                      "auto"},

    {"spools.empty_weight_hint", "weight of the empty spool in grams", "gewicht van de lege spoel in gram"},

    {"hist.unarchive",        "Unarchive", "Herstel"},
    {"files.show_preview",    "Show preview", "Toon voorbeeld"},
    {"spools.external",       "External spool", "Externe spoel"},
    {"scale.stable",          "stable", "stabiel"},
    {"scale.measuring",       "measuring…", "…meten"},
    {"scale.static_ip",       "static IP", "vast IP"},
    {"spools.selected",       "selected", "geselecteerd"},
    {"spools.weighed",        "Weighed", "Gewogen"},

    {"spools.weigh_hint",     "= weighed - empty spool", "= gewogen - leeg-spoel"},
    {"spools.name_ph",        "e.g. Bambu PLA Black", "bv. Bambu PLA Zwart"},
    {"spools.note_ph",        "e.g. dried 3/7", "bv. gedroogd 3/7"},
    {"spools.empty_ph",       "e.g. Bambu reusable", "bv. Bambu herbruikbaar"},
    {"spools.search",         "Search by name or material…", "Zoek op naam of materiaal…"},
    {"all",                   "all", "alles"},
    {"spools.export_hint",    "Just your <b>rolls + empty spools</b>, as a separate file. Handy to share, or to take over from another tablet. Importing <b>adds</b> - it overwrites nothing.", "Alleen je <b>rollen + lege spoelen</b>, als los bestand. Handig om te delen of van een andere tablet over te nemen. Importeren <b>voegt toe</b> - het overschrijft niets."},
    {"spools.export_btn",     "Export rolls", "Exporteer rollen"},
    {"spools.import_btn",     "Import rolls", "Importeer rollen"},
    {"spools.full_bk_hint",   "A full backup (including history and statistics) lives under <b>Settings</b>.", "Een volledige back-up (incl. historie en statistieken) staat bij <b>Settings</b>."},
    {"set.code_empty",        "leave empty = unchanged", "laat leeg = ongewijzigd"},
    {"set.ntfy_ph",           "e.g. printward-secret-x9k2", "bv. printward-geheim-x9k2"},
    {"set.ntfy_hint",         "Install the free <b>ntfy</b> app (or ntfy.sh in the browser) and subscribe to this topic. You get a notification when a print finishes or fails, when filament runs short, and when a weighed roll drops below the threshold.", "Installeer de gratis <b>ntfy</b>-app (of ntfy.sh in de browser) en abonneer op dit topic. Je krijgt een melding bij print klaar/mislukt, filament tekort en als een gewogen rol onder de drempel komt."},
    {"set.backup_hint",       "Everything on the tablet in one file: rolls, empty spools, weights, history and statistics. (Printer IP/serial/access code are <b>not</b> included.)", "Alles op de tablet in een bestand: rollen, lege spoelen, gewichten, historie en statistieken. (Printer-IP/serial/toegangscode zitten er <b>niet</b> in.)"},
    {"set.bk_download",       "Download backup", "Download back-up"},
    {"set.bk_restore",        "Restore everything", "Herstel alles"},
    {"set.restore_warn",      "restoring <b>overwrites</b> your current rolls, history and statistics", "herstel <b>overschrijft</b> je huidige rollen, historie en statistieken"},
    {"set.rolls_only_hint",   "Only want to share or take over your rolls? That is under <b>Spools</b>.", "Alleen je rollen delen/overnemen? Dat staat bij <b>Spools</b>."},
    {"hist.archive",          "Archive", "Archiveer"},

    // --- server replies / blocked reasons ---
    {"invalid",               "invalid",                   "ongeldig"},
    {"error",                 "error",                     "fout"},
    {"bk.restore_failed",     "restore failed",            "herstel mislukt"},
    {"bk.restored",           "restored - restart the app to load everything", "hersteld - herstart de app om alles te laden"},
    {"write_failed",          "write failed",              "schrijven mislukt"},
    {"no_path",               "no path",                   "geen pad"},
    {"deleted",               "deleted",                   "verwijderd"},
    {"unknown_cmd",           "unknown command",           "onbekend commando"},
    {"sent",                  "sent",                      "verstuurd"},
    {"print_start_sent",      "print start sent",          "print-start verstuurd"},
    {"saved",                 "saved",                     "opgeslagen"},
    {"loaded",                "loaded",                    "geladen"},
    {"test_sent",             "test notification sent",    "testmelding verstuurd"},
    {"move.blocked_printing", "Printing - motion disabled", "Bezig met printen - bewegen uitgeschakeld"},
    {"move.nozzle_cold",    "Nozzle too cold (<170\xC2\xB0" "C) - heat it first", "Nozzle te koud (<170\xC2\xB0" "C) - eerst opwarmen"},

    // --- dialogs / status messages ---
    {"set.connecting",        "connecting…",        "verbinden…"},
    {"set.sending",           "sending…",           "versturen…"},
    {"invalid_number",        "invalid number",            "ongeldig getal"},
    {"set.threshold_saved",   "threshold saved",           "drempel opgeslagen"},
    {"spools.in_slot",        "in slot",                   "in slot"},

    {"empties.none_yet",      "No empty spools yet.",      "Nog geen lege spoelen."},
    {"bk.downloaded",         "backup downloaded",         "back-up gedownload"},
    {"roll.exported",         "rolls exported",            "rollen geexporteerd"},
    {"invalid_file",          "Invalid file.",             "Ongeldig bestand."},
    {"roll.none_in_file",     "No rolls in this file.",    "Geen rollen in dit bestand."},
    {"roll.import_confirm",   "roll(s) - add to your library? Existing rolls are kept.", "rol(len) toevoegen aan je bibliotheek? Bestaande rollen blijven staan."},
    {"roll.imported",         "rolls added.",              "rollen toegevoegd."},
    {"import_failed",         "import failed",             "importeren mislukt"},
    {"bk.restore_confirm",    "Restore the full backup? This OVERWRITES your current rolls, weights, history and statistics on the tablet.", "Volledige back-up terugzetten? Dit OVERSCHRIJFT je huidige rollen, gewichten, historie en statistieken op de tablet."},
    {"diag.offline",          "offline",                   "offline"},

    {"hist.all_time",         "All time",                  "Aller tijden"},
    {"hist.prints_done",      "prints completed",          "prints voltooid"},
    {"hist.spend",            "spend",                     "uitgave"},
    {"diag.nothing_yet",      "nothing received yet",      "nog niets ontvangen"},
    {"ago",                   "ago",                       "geleden"},
    {"diag.set",              "set",                       "ingesteld"},
    {"diag.missing",          "missing",                   "ontbreekt"},
    {"diag.scale_ip",         "Scale IP",                  "Weegschaal-IP"},
    {"diag.web_url",          "Web address",               "Webadres"},
    {"diag.data_files",       "Data files (on the tablet)", "Databestanden (op de tablet)"},
    {"diag.changed",          "changed",                   "gewijzigd"},
    {"bk.never",              "No backup has ever been downloaded.", "Nog nooit een back-up gedownload."},
    {"bk.only_tablet",        "Your data only exists on the tablet.", "Je data staat alleen op de tablet."},
    {"bk.last",               "Last backup",               "Laatste back-up"},
    {"bk.time_for_new",       "time for a new one.",       "tijd voor een nieuwe."},
    {"today",                 "today",                     "vandaag"},
    {"confirm.del_items",     "item(s) - delete permanently?", "item(s) definitief verwijderen?"},
    {"confirm.del_files",     "file(s) - delete from the printer permanently?", "bestand(en) definitief van de printer verwijderen?"},
    {"confirm.del_roll",      "Delete this roll?",         "Rol verwijderen?"},
    {"confirm.del_rolls",     "rolls - delete?",           "rollen verwijderen?"},
    {"confirm.delete",        "Delete?",                   "Verwijderen?"},
    {"preview_loading",       "loading preview…",   "voorbeeld laden…"},
    {"preview_none",          "no preview available",      "geen voorbeeld beschikbaar"},
    {"dash.no_ams",           "no AMS",                    "geen AMS"},
    {"spools.none_yet",       "No rolls yet - create them on the Spools tab.", "Nog geen rollen - maak ze aan op de Spools-tab."},
    {"set.code_set",          "(set - leave empty to keep)", "(ingesteld, laat leeg = ongewijzigd)"},
    {"no_tablet",             "no tablet",                 "geen tablet"},
    {"scale.no_conn",         "no connection to the scale", "geen verbinding met schaal"},
    {"scale.ap_mode",         "AP mode",                   "AP-modus"},
    {"scale.saved_tablet",    "saved on the tablet",       "opgeslagen op tablet"},
    {"scale.no_ip_tab",       "No scale IP known - set it on the Scale tab.", "Geen schaal-IP bekend - stel het in op de Scale-tab."},
    {"scale.no_conn_alert",   "No connection to the scale.", "Geen verbinding met de schaal."},
    {"scale.no_ip",           "No scale IP known.",        "Geen schaal-IP bekend."},

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
    {"set.lang_hint",       "Applies to the tablet and this page. Adding a language? Drop a printward_lang_&lt;code&gt;.conf on the tablet - see lang/README.md.",
                            "Geldt voor de tablet en deze pagina. Een taal toevoegen? Zet een printward_lang_&lt;code&gt;.conf op de tablet - zie lang/README.md."},
    {"set.lang_saved",      "Language saved.",        "Taal opgeslagen."},
    {"set.scale",           "Printward Scale",        "Printward Scale"},
    {"set.screensaver",     "Screensaver",            "Screensaver"},
};
static const int NBUILTIN = (int)(sizeof(BUILTIN) / sizeof(BUILTIN[0]));

// --- runtime overrides (from /sdcard/printward_lang_<code>.conf) -----------
#define OVR_MAX 400
// val is sized for the longest built-in text, not the average one: the ntfy and
// backup hints are a couple of sentences, and a translation tends to run longer
// than the English. ~147 KB of static RAM, which is nothing on the tablet, and
// it keeps the drop-in .conf able to override every string rather than most.
struct Ovr { char key[48]; char val[320]; };
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
    char line[512];   // key + '=' + val, with room to spare
    int skipped = 0;
    while (fgets(line, sizeof(line), f) && g_ovr_n < OVR_MAX) {
        char* nl = strpbrk(line, "\r\n");
        if (nl) *nl = 0;
        else {
            // The line outran the buffer. Drop the tail, or the leftover would
            // come back as a line of its own and be parsed as a bogus key.
            int c; while ((c = fgetc(f)) != EOF && c != '\n') {}
        }
        if (!line[0] || line[0] == '#') continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        // Translations are written by hand, so say when one does not fit rather
        // than truncating it silently.
        if (strlen(line) >= sizeof(g_ovr[0].key) - 1)
            Serial.printf("LANG: %s: key '%s' over %d chars - truncated\n",
                          code, line, (int)sizeof(g_ovr[0].key) - 1);
        if (strlen(eq + 1) >= sizeof(g_ovr[0].val) - 1)
            Serial.printf("LANG: %s: text for '%s' over %d bytes - truncated\n",
                          code, line, (int)sizeof(g_ovr[0].val) - 1);
        strncpy(g_ovr[g_ovr_n].key, line, sizeof(g_ovr[0].key) - 1);
        g_ovr[g_ovr_n].key[sizeof(g_ovr[0].key) - 1] = 0;
        strncpy(g_ovr[g_ovr_n].val, eq + 1, sizeof(g_ovr[0].val) - 1);
        g_ovr[g_ovr_n].val[sizeof(g_ovr[0].val) - 1] = 0;
        g_ovr_n++;
    }
    if (g_ovr_n >= OVR_MAX && fgets(line, sizeof(line), f)) skipped = 1;
    fclose(f);
    Serial.printf("LANG: %s - %d string(s) from %s\n", code, g_ovr_n, path);
    if (skipped)
        Serial.printf("LANG: %s - file has more than %d strings, the rest is ignored\n",
                      code, OVR_MAX);
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
    // plus every printward_lang_<code>.conf on the tablet
    DIR* d = opendir("/sdcard");
    if (!d) return n;
    struct dirent* e;
    while ((e = readdir(d)) && n < max) {
        const char* p = strstr(e->d_name, "printward_lang_");
        if (!p || p != e->d_name) continue;
        const char* code = e->d_name + strlen("printward_lang_");
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
