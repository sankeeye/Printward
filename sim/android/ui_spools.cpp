// "Spools" screen (tablet only): list the spool library and load a roll into an
// AMS slot. Adding/editing rolls is done on the web (easier to type); this
// screen is the quick "tap a roll -> into slot" action on the tablet itself.
#include "ui_spools.h"
#include "ui_printer.h"
#include "spool_db.h"
#include <lvgl.h>
#include <cstdio>
#include "ui_scale.h"

static lv_obj_t* g_screen = nullptr;
static lv_obj_t* g_msg = nullptr;
static lv_obj_t* g_row_dd[SPOOL_MAX] = {nullptr};

static void back_cb(lv_event_t*) { lv_scr_load(g_main_screen); }

static void load_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= g_spool_count) return;
    int sel = g_row_dd[idx] ? (int)lv_dropdown_get_selected(g_row_dd[idx]) : 0;
    int slot = (sel >= 8) ? 254 : sel;         // 0..7 = AMS1/2 trays, 8 = external
    spool_load_to_slot(idx, slot);
    if (g_msg) lv_label_set_text_fmt(g_msg, "'%s' geladen in slot", g_spools[idx].name);
}

void create_spools_ui() {
    if (g_screen) { lv_obj_del(g_screen); g_screen = nullptr; }
    for (int i = 0; i < SPOOL_MAX; i++) g_row_dd[i] = nullptr;

    g_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_screen, lv_color_hex(0x101418), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(g_screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_set_size(header, lv_pct(100), PT_SZ(40));
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Spools");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_t* back = lv_btn_create(header);
    lv_obj_set_size(back, PT_SZ(80), PT_SZ(34));
    lv_obj_set_style_bg_color(back, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* bl = lv_label_create(back); lv_label_set_text(bl, "Back");
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0); lv_obj_center(bl);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* hint = lv_label_create(root);
    lv_label_set_text(hint, "Toevoegen/bewerken via de website. Tik Laad om een rol in een slot te zetten.");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x93a0ad), 0);

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
        lv_label_set_text(empty, "Nog geen rollen - voeg ze toe via http://<tablet>:8080 (Spools).");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x777777), 0);
    }

    for (int i = 0; i < g_spool_count; i++) {
        Spool& s = g_spools[i];
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), PT_SZ(54));
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1c2229), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(row, PT_SZ(10), LV_PART_MAIN);
        lv_obj_set_style_pad_ver(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_gap(row, PT_SZ(8), LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* sw = lv_obj_create(row);
        lv_obj_set_size(sw, PT_SZ(34), PT_SZ(24));
        lv_obj_set_style_radius(sw, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(s.color & 0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(sw, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_clear_flag(sw, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text_fmt(lbl, "%s  %s  %.0f g", s.name, s.material, s.remaining_g);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_flex_grow(lbl, 1);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);

        lv_obj_t* dd = lv_dropdown_create(row);
        lv_dropdown_set_options_static(dd,
            "AMS1 T1\nAMS1 T2\nAMS1 T3\nAMS1 T4\nAMS2 T1\nAMS2 T2\nAMS2 T3\nAMS2 T4\nExtern");
        lv_obj_set_width(dd, PT_SZ(120));
        lv_obj_set_style_text_font(dd, &lv_font_montserrat_14, 0);
        g_row_dd[i] = dd;

        lv_obj_t* btn = lv_btn_create(row);
        lv_obj_set_size(btn, PT_SZ(90), PT_SZ(40));
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x27ae60), LV_PART_MAIN);
        lv_obj_t* bl2 = lv_label_create(btn); lv_label_set_text(bl2, "Laad");
        lv_obj_set_style_text_font(bl2, &lv_font_montserrat_14, 0); lv_obj_center(bl2);
        lv_obj_add_event_cb(btn, load_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    g_msg = lv_label_create(root);
    lv_label_set_text(g_msg, "");
    lv_obj_set_style_text_font(g_msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_msg, lv_color_hex(0x2ecc71), 0);

    lv_scr_load(g_screen);
}
