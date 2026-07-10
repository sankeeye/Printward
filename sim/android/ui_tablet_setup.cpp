// Android-only on-screen printer configuration. Mirrors ui_wifi.cpp's pattern:
// tap a field -> on-screen keyboard; "Save" defers the slow work (file write +
// MQTT reconnect) to tablet_setup_loop() so it never runs inside a click cb.
#include "ui_tablet_setup.h"
#include "ui_printer.h"
#include "ui_settings.h"
#include "storage.h"
#include "bambu_mqtt.h"
#include <lvgl.h>
#include <cstring>
#include "ui_scale.h"   // tablet font scaling (Android only)

static lv_obj_t* g_setup_screen = nullptr;
static lv_obj_t* g_ip_ta = nullptr;
static lv_obj_t* g_serial_ta = nullptr;
static lv_obj_t* g_code_ta = nullptr;
static lv_obj_t* g_kb = nullptr;
static lv_obj_t* g_status_label = nullptr;

// See ui_wifi.cpp: save_settings() + bambu_mqtt_settings_changed() are slow
// enough to tear a frame if run straight from the click callback, so queue it.
static volatile bool g_save_requested = false;

static void back_btn_cb(lv_event_t* e) {
    (void)e;
    create_settings_ui();
}

static void ta_click_cb(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    lv_keyboard_set_textarea(g_kb, ta);
    lv_obj_clear_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_kb);
}

static void kb_event_cb(lv_event_t* e) {
    (void)e;
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(g_kb, NULL);
}

static void save_btn_cb(lv_event_t* e) {
    (void)e;
    g_save_requested = true;
    if (g_status_label) lv_label_set_text(g_status_label, "Saving...");
}

static lv_obj_t* add_field(lv_obj_t* parent, const char* label, const char* value) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xBBBBBB), 0);

    lv_obj_t* ta = lv_textarea_create(parent);
    lv_obj_set_width(ta, lv_pct(100));
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, value ? value : "");
    lv_obj_add_event_cb(ta, ta_click_cb, LV_EVENT_CLICKED, NULL);
    return ta;
}

void create_tablet_setup_ui() {
    if (g_setup_screen) {
        lv_obj_del(g_setup_screen);
        g_setup_screen = nullptr;
    }

    g_setup_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_setup_screen, lv_color_hex(0x101418), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(g_setup_screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Header: title + Back ---
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_set_size(header, lv_pct(100), PT_SZ(34));
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Printer setup");
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

    // --- Fields (prefilled with current values) ---
    g_ip_ta     = add_field(root, "Printer IP",     g_printer_ip);
    g_serial_ta = add_field(root, "Printer serial", g_printer_serial);
    g_code_ta   = add_field(root, "Access code",    g_printer_access_code);

    // --- Save + status ---
    lv_obj_t* save_btn = lv_btn_create(root);
    lv_obj_set_size(save_btn, lv_pct(100), PT_SZ(44));
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x2ecc71), LV_PART_MAIN);
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save & connect");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_14, 0);
    lv_obj_center(save_label);
    lv_obj_add_event_cb(save_btn, save_btn_cb, LV_EVENT_CLICKED, NULL);

    g_status_label = lv_label_create(root);
    lv_label_set_text(g_status_label, "");
    lv_obj_set_style_text_font(g_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(0x999999), 0);

    // --- On-screen keyboard, hidden until a field is tapped ---
    g_kb = lv_keyboard_create(g_setup_screen);
    lv_obj_set_size(g_kb, lv_pct(100), PT_SZ(200));
    lv_obj_align(g_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_kb, kb_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_kb, kb_event_cb, LV_EVENT_CANCEL, NULL);

    lv_scr_load(g_setup_screen);
}

void tablet_setup_loop() {
    if (!g_save_requested) return;
    g_save_requested = false;

    strncpy(g_printer_ip, lv_textarea_get_text(g_ip_ta), sizeof(g_printer_ip) - 1);
    g_printer_ip[sizeof(g_printer_ip) - 1] = '\0';
    strncpy(g_printer_serial, lv_textarea_get_text(g_serial_ta), sizeof(g_printer_serial) - 1);
    g_printer_serial[sizeof(g_printer_serial) - 1] = '\0';
    strncpy(g_printer_access_code, lv_textarea_get_text(g_code_ta), sizeof(g_printer_access_code) - 1);
    g_printer_access_code[sizeof(g_printer_access_code) - 1] = '\0';

    save_settings();               // persist to /sdcard/pandatouch.conf
    bambu_mqtt_settings_changed();  // rebuild topics + reconnect with new creds

    lv_scr_load(g_main_screen);     // back to the printer screen
}
