#include "ui_filament.h"
#include "ui_printer.h"
#include "bambu_mqtt.h"
#include "storage.h"
#include <lvgl.h>
#include "ui_scale.h"   // tablet font scaling (Android only)
#ifdef __ANDROID__
#include "spool_db.h"   // pick a saved roll for an AMS slot (tablet)
#include <cstdint>
#endif

static lv_obj_t* g_filament_screen = nullptr;

static lv_obj_t* g_unit_boxes[AMS_MAX_UNITS] = {nullptr};
static lv_obj_t* g_unit_humidity_labels[AMS_MAX_UNITS] = {nullptr};
static lv_obj_t* g_tray_swatches[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{nullptr}};
static lv_obj_t* g_tray_type_labels[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{nullptr}};
static lv_obj_t* g_tray_bars[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{nullptr}};
static lv_obj_t* g_tray_pct_labels[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{nullptr}};

static lv_obj_t* g_ext_row = nullptr;
static lv_obj_t* g_ext_swatch = nullptr;
static lv_obj_t* g_ext_type_label = nullptr;
static lv_obj_t* g_ext_bar = nullptr;
static lv_obj_t* g_ext_pct_label = nullptr;

// --- "Set roll weight" modal, shared by every tray/external Set button ---
// Encodes which slot is being edited: 0..(AMS_MAX_UNITS*AMS_MAX_TRAYS-1) for
// an AMS tray (u*AMS_MAX_TRAYS+t), or EXT_SLOT for the external spool.
#define EXT_SLOT 254
static int g_editing_slot = -1;
static lv_obj_t* g_modal = nullptr;
static lv_obj_t* g_modal_label = nullptr;
static lv_obj_t* g_modal_ta = nullptr;
static lv_obj_t* g_modal_keyboard = nullptr;

static void back_btn_cb(lv_event_t* e) {
    lv_scr_load(g_main_screen);
}

// Fills the swatch/type/bar/percent for one tray cell. Manual gram-based
// tracking (capacity_g > 0) takes priority over Bambu's own RFID-based
// remain% when both are available, since it's what the user explicitly told
// us; remain of -1 with no manual capacity set shows a dash rather than a
// misleading 0%.
static void set_tray_display(lv_obj_t* swatch, lv_obj_t* type_label, lv_obj_t* bar, lv_obj_t* pct_label,
                              bool present, uint32_t color, const char* type, int remain,
                              float capacity_g, float used_g) {
    if (!present) {
        lv_obj_set_style_bg_color(swatch, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(swatch, LV_OPA_50, LV_PART_MAIN);
        lv_label_set_text(type_label, "Empty");
        lv_obj_set_style_text_color(type_label, lv_color_hex(0x555555), 0);
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(pct_label, "");
        return;
    }

    lv_obj_set_style_bg_color(swatch, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, LV_PART_MAIN);
    lv_label_set_text(type_label, type);
    lv_obj_set_style_text_color(type_label, lv_color_hex(0xFFFFFF), 0);

    if (capacity_g > 0) {
        float remaining_g = capacity_g - used_g;
        if (remaining_g < 0) remaining_g = 0;
        int pct = (int)((remaining_g / capacity_g) * 100.0f + 0.5f);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(bar, pct, LV_ANIM_OFF);
        uint32_t col = pct > 40 ? 0x2ecc71 : (pct > 15 ? 0xf39c12 : 0xe74c3c);
        lv_obj_set_style_bg_color(bar, lv_color_hex(col), LV_PART_INDICATOR);
        lv_label_set_text_fmt(pct_label, "%.0fg (%d%%)", remaining_g, pct);
    } else if (remain >= 0) {
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(bar, remain, LV_ANIM_OFF);
        uint32_t col = remain > 40 ? 0x2ecc71 : (remain > 15 ? 0xf39c12 : 0xe74c3c);
        lv_obj_set_style_bg_color(bar, lv_color_hex(col), LV_PART_INDICATOR);
        lv_label_set_text_fmt(pct_label, "%d%%", remain);
    } else {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(pct_label, "-");
    }
}

static void open_weight_modal(int slot) {
    g_editing_slot = slot;
    if (slot == EXT_SLOT) {
        lv_label_set_text(g_modal_label, "External spool - new roll weight (g):");
    } else {
        int u = slot / AMS_MAX_TRAYS, t = slot % AMS_MAX_TRAYS;
        lv_label_set_text_fmt(g_modal_label, "AMS %d tray %d - new roll weight (g):", u + 1, t + 1);
    }
    lv_textarea_set_text(g_modal_ta, "");
    lv_obj_clear_flag(g_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_modal);
}

static void close_weight_modal() {
    lv_obj_add_flag(g_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_modal_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(g_modal_keyboard, NULL);
    g_editing_slot = -1;
}

static void modal_ta_event_cb(lv_event_t* e) {
    lv_keyboard_set_textarea(g_modal_keyboard, g_modal_ta);
    lv_obj_clear_flag(g_modal_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_modal_keyboard);
}

static void modal_cancel_cb(lv_event_t* e) {
    close_weight_modal();
}

// Setting a capacity means "I just loaded a fresh roll" - so used_g resets
// to 0 too (a fresh roll is 100% full by definition).
static void modal_save_cb(lv_event_t* e) {
    float grams = atof(lv_textarea_get_text(g_modal_ta));
    if (grams > 0 && g_editing_slot >= 0) {
        if (g_editing_slot == EXT_SLOT) {
            g_ext_capacity_g = grams;
            g_ext_used_g = 0;
            save_ext_weight();
        } else {
            int u = g_editing_slot / AMS_MAX_TRAYS, t = g_editing_slot % AMS_MAX_TRAYS;
            g_tray_capacity_g[u][t] = grams;
            g_tray_used_g[u][t] = 0;
            save_tray_weight(u, t);
        }
        update_filament_ui();
    }
    close_weight_modal();
}

static void set_btn_cb(lv_event_t* e) {
    int slot = (int)(uintptr_t)lv_event_get_user_data(e);
    open_weight_modal(slot);
}

#ifdef __ANDROID__
// A pop-up roll picker: tap a saved roll to load it into this AMS slot (weight
// tracking + printer AMS). This is the primary place to assign filament, right
// where the slots are shown; the Spools tab is the library that feeds it.
static lv_obj_t* g_spool_modal = nullptr;
static lv_obj_t* g_spool_list = nullptr;
static lv_obj_t* g_spool_title = nullptr;
static int g_pick_slot = -1;

static void spool_choose_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= 0 && i < g_spool_count && g_pick_slot >= 0) spool_load_to_slot(i, g_pick_slot);
    if (g_spool_modal) lv_obj_add_flag(g_spool_modal, LV_OBJ_FLAG_HIDDEN);
    update_filament_ui();
}
static void spool_modal_close_cb(lv_event_t*) {
    if (g_spool_modal) lv_obj_add_flag(g_spool_modal, LV_OBJ_FLAG_HIDDEN);
}
static void open_spool_modal(int slot) {
    if (!g_spool_modal) return;
    g_pick_slot = slot;
    if (g_spool_title) {
        if (slot == 254) lv_label_set_text(g_spool_title, "Kies rol voor de externe spoel");
        else lv_label_set_text_fmt(g_spool_title, "Kies rol voor AMS%d T%d",
                                   slot / AMS_MAX_TRAYS + 1, slot % AMS_MAX_TRAYS + 1);
    }
    lv_obj_clean(g_spool_list);
    if (g_spool_count == 0) {
        lv_obj_t* l = lv_label_create(g_spool_list);
        lv_label_set_text(l, "Nog geen rollen - maak ze aan op de Spools-tab.");
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0x999999), 0);
    }
    for (int i = 0; i < g_spool_count; i++) {
        Spool& s = g_spools[i];
        lv_obj_t* b = lv_btn_create(g_spool_list);
        lv_obj_set_size(b, lv_pct(100), PT_SZ(48));
        lv_obj_set_style_bg_color(b, lv_color_hex(0x2a323b), LV_PART_MAIN);
        lv_obj_set_style_pad_hor(b, PT_SZ(10), LV_PART_MAIN);
        lv_obj_set_style_pad_gap(b, PT_SZ(10), LV_PART_MAIN);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* sw = lv_obj_create(b);
        lv_obj_set_size(sw, PT_SZ(30), PT_SZ(24));
        lv_obj_set_style_radius(sw, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(s.color & 0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(sw, 0, LV_PART_MAIN);
        lv_obj_clear_flag(sw, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text_fmt(l, "%s   %s   %.0f g", s.name, s.material, s.remaining_g);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_add_event_cb(b, spool_choose_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
    lv_obj_clear_flag(g_spool_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_spool_modal);
}
static void spool_pick_cb(lv_event_t* e) {
    open_spool_modal((int)(uintptr_t)lv_event_get_user_data(e));
}
#endif

static lv_obj_t* make_tray_cell(lv_obj_t* parent, int slot, lv_obj_t** swatch_out, lv_obj_t** type_out,
                                 lv_obj_t** bar_out, lv_obj_t** pct_out) {
    lv_obj_t* cell = lv_obj_create(parent);
    lv_obj_set_size(cell, PT_SZ(170), PT_SZ(100));
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cell, PT_SZ(4), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(cell, PT_SZ(4), LV_PART_MAIN);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* row = lv_obj_create(cell);
    lv_obj_set_size(row, lv_pct(100), PT_SZ(30));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(row, PT_SZ(8), LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* swatch = lv_obj_create(row);
    lv_obj_set_size(swatch, PT_SZ(40), PT_SZ(26));
    lv_obj_set_style_radius(swatch, 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(swatch, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(swatch, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* type_label = lv_label_create(row);
    lv_label_set_text(type_label, "-");
    lv_obj_set_style_text_font(type_label, &lv_font_montserrat_14, 0);

    lv_obj_t* bar = lv_bar_create(cell);
    lv_obj_set_size(bar, PT_SZ(150), PT_SZ(10));
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x333333), LV_PART_MAIN);

    lv_obj_t* pct_label = lv_label_create(cell);
    lv_label_set_text(pct_label, "");
    lv_obj_set_style_text_font(pct_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pct_label, lv_color_hex(0x999999), 0);

    lv_obj_t* btn_row = lv_obj_create(cell);
    lv_obj_set_size(btn_row, lv_pct(100), PT_SZ(24));
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(btn_row, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* set_btn = lv_btn_create(btn_row);
    lv_obj_set_size(set_btn, PT_SZ(80), PT_SZ(22));
    lv_obj_set_style_bg_color(set_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* set_label = lv_label_create(set_btn);
    lv_label_set_text(set_label, "Set g");
    lv_obj_set_style_text_font(set_label, &lv_font_montserrat_12, 0);
    lv_obj_center(set_label);
    lv_obj_add_event_cb(set_btn, set_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)slot);
#ifdef __ANDROID__
    lv_obj_t* roll_btn = lv_btn_create(btn_row);
    lv_obj_set_size(roll_btn, PT_SZ(80), PT_SZ(22));
    lv_obj_set_style_bg_color(roll_btn, lv_color_hex(0x3465a4), LV_PART_MAIN);
    lv_obj_t* roll_label = lv_label_create(roll_btn);
    lv_label_set_text(roll_label, "Rol");
    lv_obj_set_style_text_font(roll_label, &lv_font_montserrat_12, 0);
    lv_obj_center(roll_label);
    lv_obj_add_event_cb(roll_btn, spool_pick_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)slot);
#endif

    *swatch_out = swatch;
    *type_out = type_label;
    *bar_out = bar;
    *pct_out = pct_label;
    return cell;
}

void create_filament_ui() {
    if (g_filament_screen) {
        lv_obj_del(g_filament_screen);
        g_filament_screen = nullptr;
    }

    g_filament_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_filament_screen, lv_color_hex(0x101418), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(g_filament_screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_center(root);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, PT_SZ(16), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    // Not scrollable, same reasoning as the Settings screen fix: a
    // scrollable column on this display's partial-refresh render mode can
    // tear the frame. Content is sized to fit a typical 1-2 AMS unit setup
    // without needing to scroll.
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Header ---
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_set_size(header, lv_pct(100), PT_SZ(36));
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Filament");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t* back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, PT_SZ(80), PT_SZ(30));
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    // --- AMS units, each a row of up to 4 tray cells ---
    for (int u = 0; u < AMS_MAX_UNITS; u++) {
        lv_obj_t* box = lv_obj_create(root);
        lv_obj_set_size(box, lv_pct(100), PT_SZ(136));
        lv_obj_set_style_bg_color(box, lv_color_hex(0x1c1c1c), LV_PART_MAIN);
        lv_obj_set_style_border_color(box, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_pad_all(box, PT_SZ(6), LV_PART_MAIN);
        lv_obj_set_style_pad_gap(box, PT_SZ(4), LV_PART_MAIN);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        g_unit_boxes[u] = box;

        lv_obj_t* unit_header = lv_obj_create(box);
        lv_obj_set_size(unit_header, lv_pct(100), PT_SZ(18));
        lv_obj_set_style_bg_opa(unit_header, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(unit_header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(unit_header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(unit_header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* unit_label = lv_label_create(unit_header);
        lv_label_set_text_fmt(unit_label, "AMS %d", u + 1);
        lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(unit_label, lv_color_hex(0x999999), 0);

        lv_obj_t* humidity_label = lv_label_create(unit_header);
        lv_label_set_text(humidity_label, "");
        lv_obj_set_style_text_font(humidity_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(humidity_label, lv_color_hex(0x777777), 0);
        lv_obj_align(humidity_label, LV_ALIGN_TOP_RIGHT, 0, 0);
        g_unit_humidity_labels[u] = humidity_label;

        lv_obj_t* tray_row = lv_obj_create(box);
        lv_obj_set_size(tray_row, lv_pct(100), PT_SZ(100));
        lv_obj_set_style_bg_opa(tray_row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(tray_row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(tray_row, 0, LV_PART_MAIN);
        lv_obj_set_flex_flow(tray_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tray_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(tray_row, LV_OBJ_FLAG_SCROLLABLE);

        for (int t = 0; t < AMS_MAX_TRAYS; t++) {
            make_tray_cell(tray_row, u * AMS_MAX_TRAYS + t, &g_tray_swatches[u][t], &g_tray_type_labels[u][t],
                           &g_tray_bars[u][t], &g_tray_pct_labels[u][t]);
        }

        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
    }

    // --- External / manual spool ---
    g_ext_row = lv_obj_create(root);
    lv_obj_set_size(g_ext_row, lv_pct(100), PT_SZ(136));
    lv_obj_set_style_bg_color(g_ext_row, lv_color_hex(0x1c1c1c), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_ext_row, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_ext_row, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(g_ext_row, PT_SZ(4), LV_PART_MAIN);
    lv_obj_set_flex_flow(g_ext_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(g_ext_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* ext_header = lv_label_create(g_ext_row);
    lv_label_set_text(ext_header, "External spool");
    lv_obj_set_style_text_font(ext_header, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ext_header, lv_color_hex(0x999999), 0);

    lv_obj_t* ext_cell_holder = lv_obj_create(g_ext_row);
    lv_obj_set_size(ext_cell_holder, lv_pct(100), PT_SZ(100));
    lv_obj_set_style_bg_opa(ext_cell_holder, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ext_cell_holder, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ext_cell_holder, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(ext_cell_holder, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ext_cell_holder, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ext_cell_holder, LV_OBJ_FLAG_SCROLLABLE);

    make_tray_cell(ext_cell_holder, EXT_SLOT, &g_ext_swatch, &g_ext_type_label, &g_ext_bar, &g_ext_pct_label);

    // --- "Set roll weight" modal (hidden until a Set button is tapped) ---
    g_modal = lv_obj_create(g_filament_screen);
    lv_obj_set_size(g_modal, PT_SZ(420), PT_SZ(160));
    lv_obj_center(g_modal);
    lv_obj_set_style_bg_color(g_modal, lv_color_hex(0x22262c), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_modal, lv_color_hex(0x3465a4), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_modal, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_modal, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(g_modal, PT_SZ(10), LV_PART_MAIN);
    lv_obj_set_flex_flow(g_modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(g_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_modal, LV_OBJ_FLAG_HIDDEN);

    g_modal_label = lv_label_create(g_modal);
    lv_label_set_text(g_modal_label, "");
    lv_obj_set_style_text_font(g_modal_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_modal_label, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_long_mode(g_modal_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_modal_label, lv_pct(100));

    g_modal_ta = lv_textarea_create(g_modal);
    lv_obj_set_size(g_modal_ta, lv_pct(100), PT_SZ(40));
    lv_textarea_set_one_line(g_modal_ta, true);
    lv_textarea_set_accepted_chars(g_modal_ta, "0123456789.");
    lv_textarea_set_placeholder_text(g_modal_ta, "e.g. 1000");
    lv_obj_add_event_cb(g_modal_ta, modal_ta_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* modal_btn_row = lv_obj_create(g_modal);
    lv_obj_set_size(modal_btn_row, lv_pct(100), PT_SZ(44));
    lv_obj_set_style_bg_opa(modal_btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal_btn_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(modal_btn_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(modal_btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(modal_btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(modal_btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* cancel_btn = lv_btn_create(modal_btn_row);
    lv_obj_set_size(cancel_btn, PT_SZ(120), PT_SZ(40));
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(cancel_btn, modal_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* save_btn = lv_btn_create(modal_btn_row);
    lv_obj_set_size(save_btn, PT_SZ(160), PT_SZ(40));
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x2ecc71), LV_PART_MAIN);
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save (fresh roll)");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_14, 0);
    lv_obj_center(save_label);
    lv_obj_add_event_cb(save_btn, modal_save_cb, LV_EVENT_CLICKED, NULL);

    // Numeric keyboard for the weight textarea, shared modal-wide.
    g_modal_keyboard = lv_keyboard_create(g_filament_screen);
    lv_keyboard_set_mode(g_modal_keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(g_modal_keyboard, lv_pct(100), PT_SZ(180));
    lv_obj_align(g_modal_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(g_modal_keyboard, LV_OBJ_FLAG_HIDDEN);

#ifdef __ANDROID__
    // Roll-picker pop-up (hidden until a tray's "Rol" button is tapped).
    g_spool_modal = lv_obj_create(g_filament_screen);
    lv_obj_set_size(g_spool_modal, PT_SZ(540), PT_SZ(400));
    lv_obj_center(g_spool_modal);
    lv_obj_set_style_bg_color(g_spool_modal, lv_color_hex(0x161b21), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_spool_modal, lv_color_hex(0x3465a4), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_spool_modal, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(g_spool_modal, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_spool_modal, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(g_spool_modal, PT_SZ(10), LV_PART_MAIN);
    lv_obj_set_flex_flow(g_spool_modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(g_spool_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_spool_modal, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* sm_head = lv_obj_create(g_spool_modal);
    lv_obj_set_size(sm_head, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(sm_head, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(sm_head, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sm_head, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(sm_head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sm_head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(sm_head, LV_OBJ_FLAG_SCROLLABLE);
    g_spool_title = lv_label_create(sm_head);
    lv_label_set_text(g_spool_title, "Kies rol");
    lv_obj_set_style_text_font(g_spool_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(g_spool_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_t* sm_close = lv_btn_create(sm_head);
    lv_obj_set_size(sm_close, PT_SZ(90), PT_SZ(34));
    lv_obj_set_style_bg_color(sm_close, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_t* sm_cl = lv_label_create(sm_close); lv_label_set_text(sm_cl, "Sluit");
    lv_obj_set_style_text_font(sm_cl, &lv_font_montserrat_14, 0); lv_obj_center(sm_cl);
    lv_obj_add_event_cb(sm_close, spool_modal_close_cb, LV_EVENT_CLICKED, NULL);

    g_spool_list = lv_obj_create(g_spool_modal);
    lv_obj_set_width(g_spool_list, lv_pct(100));
    lv_obj_set_flex_grow(g_spool_list, 1);
    lv_obj_set_style_bg_opa(g_spool_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_spool_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_spool_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(g_spool_list, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_flex_flow(g_spool_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(g_spool_list, LV_DIR_VER);
#endif

    lv_scr_load(g_filament_screen);
    update_filament_ui();
}

void update_filament_ui() {
    if (!g_filament_screen) return;

    PrinterStatus& s = g_printer_status;

    for (int u = 0; u < AMS_MAX_UNITS; u++) {
        AmsUnit& unit = s.ams[u];
        if (u >= s.ams_count || !unit.present) {
            lv_obj_add_flag(g_unit_boxes[u], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(g_unit_boxes[u], LV_OBJ_FLAG_HIDDEN);
        if (g_unit_humidity_labels[u]) {
            // Bambu humidity grade 1-5: 1 = dry (good), 5 = wet (bad). Show a
            // clear label + colour instead of the bare number.
            int hu = unit.humidity;
            if (hu >= 1) {
                const char* lbl; uint32_t col;
                if (hu <= 2)      { lbl = "droog";    col = 0x2ecc71; }
                else if (hu == 3) { lbl = "redelijk"; col = 0xf39c12; }
                else              { lbl = "vochtig";  col = 0xe74c3c; }
                lv_label_set_text_fmt(g_unit_humidity_labels[u], "Vocht: %s", lbl);
                lv_obj_set_style_text_color(g_unit_humidity_labels[u], lv_color_hex(col), 0);
            } else {
                lv_label_set_text(g_unit_humidity_labels[u], "");
            }
        }
        for (int t = 0; t < AMS_MAX_TRAYS; t++) {
            AmsTraySlot& slot = unit.trays[t];
            set_tray_display(g_tray_swatches[u][t], g_tray_type_labels[u][t],
                              g_tray_bars[u][t], g_tray_pct_labels[u][t],
                              slot.present, slot.color, slot.type, slot.remain,
                              g_tray_capacity_g[u][t], g_tray_used_g[u][t]);
        }
    }

    set_tray_display(g_ext_swatch, g_ext_type_label, g_ext_bar, g_ext_pct_label,
                      s.external_spool.present, s.external_spool.color,
                      s.external_spool.type, s.external_spool.remain,
                      g_ext_capacity_g, g_ext_used_g);
}
