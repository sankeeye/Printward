#include "ui_filament.h"
#include "ui_printer.h"
#include "bambu_mqtt.h"
#include "storage.h"
#include <lvgl.h>

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

static lv_obj_t* make_tray_cell(lv_obj_t* parent, int slot, lv_obj_t** swatch_out, lv_obj_t** type_out,
                                 lv_obj_t** bar_out, lv_obj_t** pct_out) {
    lv_obj_t* cell = lv_obj_create(parent);
    lv_obj_set_size(cell, 170, 100);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cell, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(cell, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* row = lv_obj_create(cell);
    lv_obj_set_size(row, lv_pct(100), 30);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(row, 8, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* swatch = lv_obj_create(row);
    lv_obj_set_size(swatch, 40, 26);
    lv_obj_set_style_radius(swatch, 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(swatch, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(swatch, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* type_label = lv_label_create(row);
    lv_label_set_text(type_label, "-");
    lv_obj_set_style_text_font(type_label, &lv_font_montserrat_14, 0);

    lv_obj_t* bar = lv_bar_create(cell);
    lv_obj_set_size(bar, 150, 10);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x333333), LV_PART_MAIN);

    lv_obj_t* pct_label = lv_label_create(cell);
    lv_label_set_text(pct_label, "");
    lv_obj_set_style_text_font(pct_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pct_label, lv_color_hex(0x999999), 0);

    lv_obj_t* set_btn = lv_btn_create(cell);
    lv_obj_set_size(set_btn, 90, 20);
    lv_obj_set_style_bg_color(set_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* set_label = lv_label_create(set_btn);
    lv_label_set_text(set_label, "Set roll");
    lv_obj_set_style_text_font(set_label, &lv_font_montserrat_12, 0);
    lv_obj_center(set_label);
    lv_obj_add_event_cb(set_btn, set_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)slot);

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
    lv_obj_set_style_pad_all(root, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    // Not scrollable, same reasoning as the Settings screen fix: a
    // scrollable column on this display's partial-refresh render mode can
    // tear the frame. Content is sized to fit a typical 1-2 AMS unit setup
    // without needing to scroll.
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Header ---
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_set_size(header, lv_pct(100), 36);
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
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    // --- AMS units, each a row of up to 4 tray cells ---
    for (int u = 0; u < AMS_MAX_UNITS; u++) {
        lv_obj_t* box = lv_obj_create(root);
        lv_obj_set_size(box, lv_pct(100), 136);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x1c1c1c), LV_PART_MAIN);
        lv_obj_set_style_border_color(box, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_pad_all(box, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_gap(box, 4, LV_PART_MAIN);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        g_unit_boxes[u] = box;

        lv_obj_t* unit_header = lv_obj_create(box);
        lv_obj_set_size(unit_header, lv_pct(100), 18);
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
        lv_obj_set_size(tray_row, lv_pct(100), 100);
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
    lv_obj_set_size(g_ext_row, lv_pct(100), 136);
    lv_obj_set_style_bg_color(g_ext_row, lv_color_hex(0x1c1c1c), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_ext_row, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_ext_row, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(g_ext_row, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(g_ext_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(g_ext_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* ext_header = lv_label_create(g_ext_row);
    lv_label_set_text(ext_header, "External spool");
    lv_obj_set_style_text_font(ext_header, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ext_header, lv_color_hex(0x999999), 0);

    lv_obj_t* ext_cell_holder = lv_obj_create(g_ext_row);
    lv_obj_set_size(ext_cell_holder, lv_pct(100), 100);
    lv_obj_set_style_bg_opa(ext_cell_holder, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ext_cell_holder, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ext_cell_holder, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(ext_cell_holder, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ext_cell_holder, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ext_cell_holder, LV_OBJ_FLAG_SCROLLABLE);

    make_tray_cell(ext_cell_holder, EXT_SLOT, &g_ext_swatch, &g_ext_type_label, &g_ext_bar, &g_ext_pct_label);

    // --- "Set roll weight" modal (hidden until a Set button is tapped) ---
    g_modal = lv_obj_create(g_filament_screen);
    lv_obj_set_size(g_modal, 420, 160);
    lv_obj_center(g_modal);
    lv_obj_set_style_bg_color(g_modal, lv_color_hex(0x22262c), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_modal, lv_color_hex(0x3465a4), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_modal, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_modal, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(g_modal, 10, LV_PART_MAIN);
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
    lv_obj_set_size(g_modal_ta, lv_pct(100), 40);
    lv_textarea_set_one_line(g_modal_ta, true);
    lv_textarea_set_accepted_chars(g_modal_ta, "0123456789.");
    lv_textarea_set_placeholder_text(g_modal_ta, "e.g. 1000");
    lv_obj_add_event_cb(g_modal_ta, modal_ta_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* modal_btn_row = lv_obj_create(g_modal);
    lv_obj_set_size(modal_btn_row, lv_pct(100), 44);
    lv_obj_set_style_bg_opa(modal_btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(modal_btn_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(modal_btn_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(modal_btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(modal_btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(modal_btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* cancel_btn = lv_btn_create(modal_btn_row);
    lv_obj_set_size(cancel_btn, 120, 40);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(cancel_btn, modal_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* save_btn = lv_btn_create(modal_btn_row);
    lv_obj_set_size(save_btn, 160, 40);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x2ecc71), LV_PART_MAIN);
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save (fresh roll)");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_14, 0);
    lv_obj_center(save_label);
    lv_obj_add_event_cb(save_btn, modal_save_cb, LV_EVENT_CLICKED, NULL);

    // Numeric keyboard for the weight textarea, shared modal-wide.
    g_modal_keyboard = lv_keyboard_create(g_filament_screen);
    lv_keyboard_set_mode(g_modal_keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(g_modal_keyboard, lv_pct(100), 180);
    lv_obj_align(g_modal_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(g_modal_keyboard, LV_OBJ_FLAG_HIDDEN);

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
            if (unit.humidity >= 0) {
                lv_label_set_text_fmt(g_unit_humidity_labels[u], "Humidity %d", unit.humidity);
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
