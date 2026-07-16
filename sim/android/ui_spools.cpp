// "Spools" screens (tablet only): the spool library list + load-into-AMS, an
// add/edit form for a roll, and an empty-spool library editor. Everything the
// web Spools tab can do, on the tablet itself.
#include "ui_spools.h"
#include "lang.h"
#include "ui_printer.h"
#include "spool_db.h"
#include "history.h"        // print log + cost per print
#include "scale_client.h"   // read the live scale weight for empty-spool weighing
#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include "ui_scale.h"

static lv_obj_t* g_screen = nullptr;
static lv_obj_t* g_msg = nullptr;
static lv_obj_t* g_kb = nullptr;

// edit-form widgets
static lv_obj_t* g_e_name = nullptr;
static lv_obj_t* g_e_mat = nullptr;
static lv_obj_t* g_e_rem = nullptr;
static lv_obj_t* g_e_empty = nullptr;
static lv_obj_t* g_e_emptysel = nullptr;
static lv_obj_t* g_e_nmin = nullptr;
static lv_obj_t* g_e_nmax = nullptr;
static lv_obj_t* g_e_code = nullptr;
static lv_obj_t* g_e_note = nullptr;
static lv_obj_t* g_e_swatch = nullptr;
static lv_obj_t* g_e_price = nullptr;
static uint32_t  g_e_color = 0x22AA55;
static int       g_edit_idx = -1;
// empty-form widgets
static lv_obj_t* g_em_name = nullptr;
static lv_obj_t* g_em_weight = nullptr;

static const char* MATERIALS = "PLA\nPETG\nABS\nTPU\nASA\nPC\nPA\nPVA";
static const uint32_t PALETTE[] = {
    0x000000, 0xFFFFFF, 0x9AA0A6, 0xC0392B, 0xE67E22, 0xF1C40F,
    0x27AE60, 0x2980B9, 0x8E44AD, 0xEC407A, 0x6D4C41, 0x16A085,
};

void create_spools_ui();
static void create_spool_edit(int idx);
static void create_empties_ui();

// --- shared helpers ------------------------------------------------------
static void kb_hide() {
    if (!g_kb) return;
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(g_kb, nullptr);
}
static void ta_focus_cb(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    bool num = (ta == g_e_rem || ta == g_e_empty || ta == g_e_nmin || ta == g_e_nmax || ta == g_em_weight);
    lv_keyboard_set_mode(g_kb, num ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(g_kb, ta);
    lv_obj_clear_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_kb);
}
static void kb_event_cb(lv_event_t* e) {
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_READY || c == LV_EVENT_CANCEL) kb_hide();
}
static lv_obj_t* mk_screen() {
    lv_obj_t* s = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s, lv_color_hex(0x101418), LV_PART_MAIN);
    return s;
}
static lv_obj_t* mk_root(lv_obj_t* screen, bool scroll) {
    lv_obj_t* root = lv_obj_create(screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    if (!scroll) lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    else lv_obj_set_scroll_dir(root, LV_DIR_VER);
    return root;
}
static lv_obj_t* mk_btn(lv_obj_t* p, const char* txt, uint32_t color, lv_event_cb_t cb, void* ud, int w, int h) {
    lv_obj_t* b = lv_btn_create(p);
    lv_obj_set_size(b, PT_SZ(w), PT_SZ(h));
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}
static lv_obj_t* mk_labeled_ta(lv_obj_t* parent, const char* label, const char* val, int w) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_set_size(box, PT_SZ(w), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(box, PT_SZ(2), LV_PART_MAIN);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lb = lv_label_create(box);
    lv_label_set_text(lb, label);
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lb, lv_color_hex(0x93a0ad), 0);
    lv_obj_t* ta = lv_textarea_create(box);
    lv_obj_set_size(ta, lv_pct(100), PT_SZ(44));
    lv_textarea_set_one_line(ta, true);
    if (val) lv_textarea_set_text(ta, val);
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_CLICKED, nullptr);
    return ta;
}

// --- list screen ---------------------------------------------------------
static void back_cb(lv_event_t*) { scale_set_polling(false); lv_scr_load(g_main_screen); }
static void new_cb(lv_event_t*) { create_spool_edit(-1); }
// Weigh the whole roll: remaining = scale - this roll's empty-spool weight.
static void rowweigh_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= g_spool_count) return;
    Spool s = g_spools[i];
    float rem = scale_grams() - s.empty_g;
    if (rem < 0) rem = 0;
    s.remaining_g = rem;
    spool_upsert(i, s);
    create_spools_ui();
    if (g_msg) lv_label_set_text_fmt(g_msg, T("spools.weighed_fmt"), s.name, rem);
}
static void empties_cb(lv_event_t*) { create_empties_ui(); }
static void edit_cb(lv_event_t* e) { create_spool_edit((int)(intptr_t)lv_event_get_user_data(e)); }
static void del_cb(lv_event_t* e) { spool_delete((int)(intptr_t)lv_event_get_user_data(e)); create_spools_ui(); }

// One row's text: name, material, slot badge, and LIVE grams (which tick down
// while a print runs on that slot) + cost. Shared by the build and the tick.
static lv_obj_t* g_row_lbl[SPOOL_MAX] = {0};
static int       g_row_n = 0;
// Rough remaining length in metres from grams, using each material's density
// (1.75 mm filament: metres = grams / (density_g_cm3 * 2.4053)).
static float roll_meters(float grams, const char* mat) {
    float d = 1.24f;   // PLA default
    if      (!strcmp(mat, "PETG")) d = 1.27f;
    else if (!strcmp(mat, "ABS"))  d = 1.04f;
    else if (!strcmp(mat, "ASA"))  d = 1.07f;
    else if (!strcmp(mat, "TPU"))  d = 1.21f;
    else if (!strcmp(mat, "PC"))   d = 1.20f;
    else if (!strcmp(mat, "PA"))   d = 1.14f;
    else if (!strcmp(mat, "PVA"))  d = 1.23f;
    return d > 0 ? grams / (d * 2.4053f) : 0.0f;
}
static void format_spool_row(char* buf, int len, const Spool& s) {
    char sl[16]; spool_slot_label(s.slot, sl, sizeof(sl));
    char slp[24] = "";
    if (sl[0]) snprintf(slp, sizeof(slp), "  [%s]", sl);
    float g = spool_live_grams(s);
    float m = roll_meters(g, s.material);
    if (s.price_kg > 0)
        snprintf(buf, len, "#%d  %s  %s%s  %.0f g (~%.0f m)  EUR %.2f", s.number, s.name, s.material, slp, g, m, g * s.price_kg / 1000.0f);
    else
        snprintf(buf, len, "#%d  %s  %s%s  %.0f g (~%.0f m)", s.number, s.name, s.material, slp, g, m);
}
// Called from the main loop (~1 Hz): refresh the visible grams without rebuilding
// the screen, so a printing slot ticks down live in the Spools list too.
void spools_live_loop() {
    if (!g_screen || lv_scr_act() != g_screen) return;
    static uint32_t last = 0;
    uint32_t now = lv_tick_get();
    if (now - last < 1000) return;
    last = now;
    for (int i = 0; i < g_row_n && i < g_spool_count; i++)
        if (g_row_lbl[i]) { char b[96]; format_spool_row(b, sizeof(b), g_spools[i]); lv_label_set_text(g_row_lbl[i], b); }
}

// --- print history / cost per project ------------------------------------
static void history_back_cb(lv_event_t*) { create_spools_ui(); }

void create_history_ui() {
    if (g_screen) { lv_obj_del(g_screen); g_screen = nullptr; }
    g_row_n = 0;   // the Spools list rows are gone now - stop spools_live_loop()
                   // from writing to their freed labels (g_screen is shared).
    g_kb = nullptr;
    scale_set_polling(false);

    g_screen = mk_screen();
    lv_obj_t* root = mk_root(g_screen, false);

    // Header: title + Back
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_set_size(header, lv_pct(100), PT_SZ(42));
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(header, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, T("hist.title"));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_flex_grow(title, 1);
    mk_btn(header, "Back", 0x333333, history_back_cb, nullptr, 80, 34);

    // Summary: number of logged prints + total cost
    lv_obj_t* sum = lv_label_create(root);
    lv_label_set_text_fmt(sum, "%d prints in het logboek   -   totaal EUR %.2f",
                          g_hist_count, g_hist_total_cost);
    lv_obj_set_style_text_font(sum, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(sum, lv_color_hex(0x2ecc71), 0);

    // Smart numbers: success rate, failed waste, averages, total print time.
    {
        int okn = 0, faln = 0; float okg = 0, okc = 0, falg = 0, falc = 0; long tott = 0;
        for (int i = 0; i < g_hist_count; i++) {
            PrintRec& r = g_history[i];
            if (r.ok) { okn++; okg += r.grams; okc += r.cost; }
            else      { faln++; falg += r.grams; falc += r.cost; }
            if (r.mins > 0) tott += r.mins;
        }
        int tot = okn + faln;
        int rate = tot > 0 ? (okn * 100 / tot) : 0;
        char dur[24];
        if (tott >= 60) snprintf(dur, sizeof(dur), "%ldu%02ld", tott / 60, tott % 60);
        else            snprintf(dur, sizeof(dur), "%ld min", tott);
        char l1[200];
        snprintf(l1, sizeof(l1),
                 "Geslaagd: %d%% (%d/%d)   Mislukt: %d (%.0f g, EUR %.2f verspild)\n"
                 "Gem./print: %.0f g  EUR %.2f   -   Printtijd totaal: %s",
                 rate, okn, tot, faln, falg, falc,
                 okn ? okg / okn : 0.0f, okn ? okc / okn : 0.0f, tott > 0 ? dur : "onbekend");
        lv_obj_t* s1 = lv_label_create(root);
        lv_label_set_text(s1, l1);
        lv_obj_set_style_text_font(s1, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s1, lv_color_hex(0xCCCCCC), 0);
    }

    // Per-material breakdown (grams + cost + count + share), heaviest first.
    {
        struct MA { char m[12]; float g; float c; int n; } agg[12];
        int na = 0;
        for (int i = 0; i < g_hist_count; i++) {
            const char* m = g_history[i].mat[0] ? g_history[i].mat : "?";
            int j;
            for (j = 0; j < na; j++) if (!strcmp(agg[j].m, m)) break;
            if (j == na && na < 12) {
                strncpy(agg[na].m, m, sizeof(agg[na].m) - 1); agg[na].m[sizeof(agg[na].m) - 1] = 0;
                agg[na].g = 0; agg[na].c = 0; agg[na].n = 0; na++;
            }
            if (j < 12) { agg[j].g += g_history[i].grams; agg[j].c += g_history[i].cost; agg[j].n++; }
        }
        if (na > 0) {
            float totg = 0; for (int j = 0; j < na; j++) totg += agg[j].g;
            char buf[480]; int p = 0; buf[0] = 0;
            for (int j = 0; j < na; j++) {
                int pc = totg > 0 ? (int)(agg[j].g / totg * 100.0f + 0.5f) : 0;
                p += snprintf(buf + p, sizeof(buf) - p, "%s%s: %.0f g   EUR %.2f (%dx, %d%%)",
                              j ? "\n" : "", agg[j].m, agg[j].g, agg[j].c, agg[j].n, pc);
            }
            lv_obj_t* ml = lv_label_create(root);
            lv_label_set_text(ml, buf);
            lv_obj_set_style_text_font(ml, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(ml, lv_color_hex(0xBBBBBB), 0);
        }
    }

    // Scrollable list, newest first
    lv_obj_t* list = lv_obj_create(root);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(list, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    if (g_hist_count == 0) {
        lv_obj_t* e = lv_label_create(list);
        lv_label_set_text(e, T("hist.log_empty"));
        lv_obj_set_style_text_color(e, lv_color_hex(0x777777), 0);
    }
    for (int i = 0; i < g_hist_count; i++) {
        PrintRec& r = g_history[i];
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), PT_SZ(46));
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1c2229), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(row, PT_SZ(10), LV_PART_MAIN);
        lv_obj_set_style_pad_ver(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_gap(row, PT_SZ(8), LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* mark = lv_label_create(row);
        lv_label_set_text(mark, r.ok ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(mark, lv_color_hex(r.ok ? 0x2ecc71 : 0xe74c3c), 0);

        lv_obj_t* nm = lv_label_create(row);
        lv_label_set_text_fmt(nm, "%s   %s", r.name, r.when);
        lv_obj_set_style_text_font(nm, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(nm, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_flex_grow(nm, 1);
        lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);

        lv_obj_t* cost = lv_label_create(row);
        if (r.cost > 0) lv_label_set_text_fmt(cost, "%.0f g   EUR %.2f", r.grams, r.cost);
        else            lv_label_set_text_fmt(cost, "%.0f g", r.grams);
        lv_obj_set_style_text_font(cost, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(cost, lv_color_hex(0xdddddd), 0);
    }

    lv_scr_load(g_screen);
}
static void history_cb(lv_event_t*) { create_history_ui(); }

void create_spools_ui() {
    if (g_screen) { lv_obj_del(g_screen); g_screen = nullptr; }
    g_kb = nullptr;
    scale_set_polling(true);   // keep the live weight fresh for the Weeg buttons

    g_screen = mk_screen();
    lv_obj_t* root = mk_root(g_screen, false);

    // Header: title + Nieuwe rol + Lege spoelen + Back
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_set_size(header, lv_pct(100), PT_SZ(42));
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(header, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, T("nav.spools"));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_flex_grow(title, 1);
    mk_btn(header, "Nieuwe rol", 0x27ae60, new_cb, nullptr, 130, 34);
    mk_btn(header, "Lege spoelen", 0x555555, empties_cb, nullptr, 140, 34);
    mk_btn(header, "Historie", 0x3465a4, history_cb, nullptr, 120, 34);
    mk_btn(header, "Back", 0x333333, back_cb, nullptr, 80, 34);

    // Inventory summary: number of rolls, total kg, total value.
    {
        float totG = 0, totV = 0;
        for (int i = 0; i < g_spool_count; i++) {
            float g = spool_live_grams(g_spools[i]);
            totG += g;
            if (g_spools[i].price_kg > 0) totV += g * g_spools[i].price_kg / 1000.0f;
        }
        lv_obj_t* inv = lv_label_create(root);
        if (totV > 0) lv_label_set_text_fmt(inv, "%d rollen   -   %.2f kg   -   EUR %.2f", g_spool_count, totG / 1000.0f, totV);
        else          lv_label_set_text_fmt(inv, "%d rollen   -   %.2f kg", g_spool_count, totG / 1000.0f);
        lv_obj_set_style_text_font(inv, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(inv, lv_color_hex(0x2ecc71), 0);
    }

    // Scrollable list of spools
    lv_obj_t* list = lv_obj_create(root);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(list, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    if (g_spool_count == 0) {
        lv_obj_t* empty = lv_label_create(list);
        lv_label_set_text(empty, T("spools.none_tap_new"));
        lv_obj_set_style_text_color(empty, lv_color_hex(0x777777), 0);
    }
    g_row_n = 0;
    for (int i = 0; i < g_spool_count; i++) {
        Spool& s = g_spools[i];
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), PT_SZ(54));
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1c2229), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(row, PT_SZ(10), LV_PART_MAIN);
        lv_obj_set_style_pad_ver(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_gap(row, PT_SZ(6), LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* sw = lv_obj_create(row);
        lv_obj_set_size(sw, PT_SZ(30), PT_SZ(22));
        lv_obj_set_style_radius(sw, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(s.color & 0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(sw, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_clear_flag(sw, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(row);
        { char buf[96]; format_spool_row(buf, sizeof(buf), s); lv_label_set_text(lbl, buf); }
        if (g_row_n < SPOOL_MAX) g_row_lbl[g_row_n++] = lbl;   // for the live tick
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_flex_grow(lbl, 1);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);

        mk_btn(row, "Weeg", 0x2980b9, rowweigh_cb, (void*)(intptr_t)i, 90, 40);
        mk_btn(row, "Bewerk", 0x3465a4, edit_cb, (void*)(intptr_t)i, 100, 40);
        mk_btn(row, "X", 0xa40000, del_cb, (void*)(intptr_t)i, 48, 40);
    }

    g_msg = lv_label_create(root);
    lv_label_set_text(g_msg, "");
    lv_obj_set_style_text_font(g_msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_msg, lv_color_hex(0x2ecc71), 0);

    lv_scr_load(g_screen);
}

// --- roll edit form ------------------------------------------------------
static void swatch_cb(lv_event_t* e) {
    g_e_color = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (g_e_swatch) lv_obj_set_style_bg_color(g_e_swatch, lv_color_hex(g_e_color), LV_PART_MAIN);
}
static void emptysel_cb(lv_event_t*) {
    int sel = (int)lv_dropdown_get_selected(g_e_emptysel);
    if (sel >= 1 && sel - 1 < g_empty_count) {
        char b[16]; snprintf(b, sizeof(b), "%.0f", g_empties[sel - 1].weight_g);
        lv_textarea_set_text(g_e_empty, b);
    }
}
// Weigh the roll now: remaining = live scale weight - the empty-spool field.
static void rem_weigh_cb(lv_event_t*) {
    float empty = (float)atof(lv_textarea_get_text(g_e_empty));
    float rem = scale_grams() - empty;
    if (rem < 0) rem = 0;
    char b[16]; snprintf(b, sizeof(b), "%.0f", rem);
    lv_textarea_set_text(g_e_rem, b);
}
static void edit_cancel_cb(lv_event_t*) { create_spools_ui(); }
static void edit_save_cb(lv_event_t*) {
    Spool s; memset(&s, 0, sizeof(s));
    strncpy(s.name, lv_textarea_get_text(g_e_name), sizeof(s.name) - 1);
    char mbuf[16]; lv_dropdown_get_selected_str(g_e_mat, mbuf, sizeof(mbuf));
    strncpy(s.material, mbuf, sizeof(s.material) - 1);
    s.color = g_e_color;
    s.remaining_g = (float)atof(lv_textarea_get_text(g_e_rem));
    s.empty_g = (float)atof(lv_textarea_get_text(g_e_empty));
    s.nmin = atoi(lv_textarea_get_text(g_e_nmin));
    s.nmax = atoi(lv_textarea_get_text(g_e_nmax));
    strncpy(s.code, lv_textarea_get_text(g_e_code), sizeof(s.code) - 1);
    strncpy(s.note, lv_textarea_get_text(g_e_note), sizeof(s.note) - 1);
    s.price_kg = (float)atof(lv_textarea_get_text(g_e_price));
    if (g_edit_idx >= 0 && g_edit_idx < g_spool_count) s.number = g_spools[g_edit_idx].number;  // keep the roll number
    spool_upsert(g_edit_idx, s);
    create_spools_ui();
}
static lv_obj_t* form_row(lv_obj_t* parent) {
    lv_obj_t* r = lv_obj_create(parent);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(r, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(r, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(r, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    return r;
}

static void create_spool_edit(int idx) {
    g_edit_idx = idx;
    Spool cur; memset(&cur, 0, sizeof(cur));
    strcpy(cur.material, "PLA"); cur.remaining_g = 1000; cur.empty_g = 250; cur.color = 0x22AA55;
    if (idx >= 0 && idx < g_spool_count) cur = g_spools[idx];
    g_e_color = cur.color & 0xFFFFFF;

    if (g_screen) { lv_obj_del(g_screen); g_screen = nullptr; }
    g_row_n = 0;   // see create_history_ui(): don't touch freed Spools-row labels
    scale_set_polling(true);   // live weight for the Weeg button
    g_screen = mk_screen();
    lv_obj_t* root = mk_root(g_screen, true);

    lv_obj_t* header = form_row(root);
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, idx < 0 ? T("spools.new_roll") : T("spools.edit_roll"));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_flex_grow(title, 1);
    mk_btn(header, "Annuleer", 0x555555, edit_cancel_cb, nullptr, 110, 40);
    mk_btn(header, "Opslaan", 0x27ae60, edit_save_cb, nullptr, 110, 40);

    g_e_name = mk_labeled_ta(root, "Naam / merk", cur.name, 460);

    lv_obj_t* r1 = form_row(root);
    lv_obj_t* mbox = lv_obj_create(r1);
    lv_obj_set_size(mbox, PT_SZ(150), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(mbox, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(mbox, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(mbox, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(mbox, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(mbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* ml = lv_label_create(mbox); lv_label_set_text(ml, T("spools.material"));
    lv_obj_set_style_text_font(ml, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ml, lv_color_hex(0x93a0ad), 0);
    g_e_mat = lv_dropdown_create(mbox);
    lv_dropdown_set_options_static(g_e_mat, MATERIALS);
    lv_obj_set_width(g_e_mat, lv_pct(100));
    lv_obj_set_style_text_font(g_e_mat, &lv_font_montserrat_14, 0);
    {   // select the current material
        const char* opt = MATERIALS; int mi = 0, sel = 0;
        char buf[16]; int bi = 0;
        for (const char* c = opt; ; c++) {
            if (*c == '\n' || *c == 0) {
                buf[bi] = 0;
                if (strcasecmp(buf, cur.material) == 0) { sel = mi; break; }
                mi++; bi = 0; if (*c == 0) break;
            } else if (bi < 15) buf[bi++] = *c;
        }
        lv_dropdown_set_selected(g_e_mat, sel);
    }
    g_e_rem = mk_labeled_ta(r1, "Resterend (g)", nullptr, 110);
    { char b[16]; snprintf(b, sizeof(b), "%.0f", cur.remaining_g); lv_textarea_set_text(g_e_rem, b); }
    mk_btn(r1, "Weeg", 0x2980b9, rem_weigh_cb, nullptr, 90, 44);

    lv_obj_t* r2 = form_row(root);
    // "Leeg spoel" group: pick from the library OR type the grams, one label
    lv_obj_t* ebox = lv_obj_create(r2);
    lv_obj_set_size(ebox, PT_SZ(320), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ebox, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ebox, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ebox, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(ebox, PT_SZ(4), LV_PART_MAIN);
    lv_obj_set_flex_flow(ebox, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(ebox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* el = lv_label_create(ebox); lv_label_set_text(el, T("spools.empty_pick"));
    lv_obj_set_style_text_font(el, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(el, lv_color_hex(0x93a0ad), 0);
    lv_obj_t* erow = lv_obj_create(ebox);
    lv_obj_set_size(erow, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(erow, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(erow, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(erow, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(erow, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_flex_flow(erow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(erow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(erow, LV_OBJ_FLAG_SCROLLABLE);
    g_e_emptysel = lv_dropdown_create(erow);
    char eopts[512]; int en = snprintf(eopts, sizeof(eopts), "uit bibliotheek...");
    for (int i = 0; i < g_empty_count && en < (int)sizeof(eopts) - 48; i++)
        en += snprintf(eopts + en, sizeof(eopts) - en, "\n%s (%.0fg)", g_empties[i].name, g_empties[i].weight_g);
    lv_dropdown_set_options(g_e_emptysel, eopts);
    lv_obj_set_flex_grow(g_e_emptysel, 1);
    lv_obj_set_style_text_font(g_e_emptysel, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(g_e_emptysel, emptysel_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    g_e_empty = lv_textarea_create(erow);
    lv_obj_set_size(g_e_empty, PT_SZ(84), PT_SZ(44));
    lv_textarea_set_one_line(g_e_empty, true);
    lv_obj_add_event_cb(g_e_empty, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(g_e_empty, ta_focus_cb, LV_EVENT_CLICKED, nullptr);
    { char b[16]; snprintf(b, sizeof(b), "%.0f", cur.empty_g); lv_textarea_set_text(g_e_empty, b); }
    lv_obj_t* eg = lv_label_create(erow); lv_label_set_text(eg, "g");
    lv_obj_set_style_text_font(eg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(eg, lv_color_hex(0x93a0ad), 0);
    g_e_nmin = mk_labeled_ta(r2, "Nozzle min", nullptr, 100);
    g_e_nmax = mk_labeled_ta(r2, "Nozzle max", nullptr, 100);
    if (cur.nmin > 0) { char b[8]; snprintf(b, sizeof(b), "%d", cur.nmin); lv_textarea_set_text(g_e_nmin, b); }
    if (cur.nmax > 0) { char b[8]; snprintf(b, sizeof(b), "%d", cur.nmax); lv_textarea_set_text(g_e_nmax, b); }

    lv_obj_t* r3 = form_row(root);
    g_e_code = mk_labeled_ta(r3, "Bambu-code (leeg = auto)", cur.code, 180);
    g_e_note = mk_labeled_ta(r3, "Notitie", cur.note, 220);
    g_e_price = mk_labeled_ta(r3, "Prijs (EUR/kg)", nullptr, 130);
    if (cur.price_kg > 0) { char b[16]; snprintf(b, sizeof(b), "%.2f", cur.price_kg); lv_textarea_set_text(g_e_price, b); }

    // colour palette
    lv_obj_t* cl = lv_label_create(root); lv_label_set_text(cl, T("spools.colour"));
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cl, lv_color_hex(0x93a0ad), 0);
    lv_obj_t* pal = form_row(root);
    lv_obj_add_flag(pal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(pal, LV_DIR_HOR);
    g_e_swatch = lv_obj_create(pal);
    lv_obj_set_size(g_e_swatch, PT_SZ(48), PT_SZ(40));
    lv_obj_set_style_radius(g_e_swatch, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_e_swatch, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_e_swatch, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_e_swatch, lv_color_hex(g_e_color), LV_PART_MAIN);
    lv_obj_clear_flag(g_e_swatch, LV_OBJ_FLAG_SCROLLABLE);
    for (uint32_t col : PALETTE) {
        lv_obj_t* sw = lv_btn_create(pal);
        lv_obj_set_size(sw, PT_SZ(40), PT_SZ(40));
        lv_obj_set_style_radius(sw, 6, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(col), LV_PART_MAIN);
        lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(sw, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_add_event_cb(sw, swatch_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)col);
    }

    g_kb = lv_keyboard_create(g_screen);
    lv_obj_set_size(g_kb, lv_pct(100), PT_SZ(170));
    lv_obj_align(g_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_kb, kb_event_cb, LV_EVENT_ALL, nullptr);

    lv_scr_load(g_screen);
}

// --- empty-spool library editor ------------------------------------------
static void empty_add_cb(lv_event_t*) {
    EmptySpool e; memset(&e, 0, sizeof(e));
    strncpy(e.name, lv_textarea_get_text(g_em_name), sizeof(e.name) - 1);
    e.weight_g = (float)atof(lv_textarea_get_text(g_em_weight));
    if (e.name[0]) empty_upsert(-1, e);
    create_empties_ui();
}
static void empty_del_cb(lv_event_t* e) { empty_delete((int)(intptr_t)lv_event_get_user_data(e)); create_empties_ui(); }
static void em_weigh_cb(lv_event_t*) {
    char b[16];
    snprintf(b, sizeof(b), "%.0f", scale_grams());
    lv_textarea_set_text(g_em_weight, b);
}
static void empties_back_cb(lv_event_t*) { scale_set_polling(false); create_spools_ui(); }

static void create_empties_ui() {
    if (g_screen) { lv_obj_del(g_screen); g_screen = nullptr; }
    g_row_n = 0;   // see create_history_ui(): don't touch freed Spools-row labels
    g_kb = nullptr;
    g_screen = mk_screen();
    lv_obj_t* root = mk_root(g_screen, false);

    lv_obj_t* header = form_row(root);
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, T("spools.empty_spools"));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_flex_grow(title, 1);
    mk_btn(header, "Terug", 0x333333, empties_back_cb, nullptr, 100, 40);

    lv_obj_t* addrow = form_row(root);
    g_em_name = mk_labeled_ta(addrow, "Naam", nullptr, 240);
    g_em_weight = mk_labeled_ta(addrow, "Gewicht (g)", "250", 110);
    mk_btn(addrow, "Weeg", 0x3465a4, em_weigh_cb, nullptr, 100, 44);
    mk_btn(addrow, "Toevoegen", 0x27ae60, empty_add_cb, nullptr, 130, 44);

    scale_set_polling(true);   // keep the live weight fresh for the Weeg button

    lv_obj_t* list = lv_obj_create(root);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(list, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    for (int i = 0; i < g_empty_count; i++) {
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), PT_SZ(48));
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1c2229), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(row, PT_SZ(10), LV_PART_MAIN);
        lv_obj_set_style_pad_ver(row, 0, LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text_fmt(lbl, "%s   %.0f g", g_empties[i].name, g_empties[i].weight_g);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_flex_grow(lbl, 1);
        mk_btn(row, "X", 0xa40000, empty_del_cb, (void*)(intptr_t)i, 44, 40);
    }

    g_kb = lv_keyboard_create(g_screen);
    lv_obj_set_size(g_kb, lv_pct(100), PT_SZ(170));
    lv_obj_align(g_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_kb, kb_event_cb, LV_EVENT_ALL, nullptr);

    lv_scr_load(g_screen);
}
