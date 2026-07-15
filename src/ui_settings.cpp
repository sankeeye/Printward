#include "ui_settings.h"
#include "ui_printer.h"
#include "ui_wifi.h"
#include "storage.h"
#include "bambu_mqtt.h"
#include "constants.h"
#include "pt/pt_display.h"
#include <lvgl.h>
#include "ui_scale.h"   // tablet font scaling (Android only)
#ifdef __ANDROID__
#include "ui_tablet_setup.h"   // on-screen printer config (tablet only)
#include "ui_screensaver.h"    // g_screensaver_3d toggle
#include "ui_weigh.h"          // FilaTrack Scale management screen (tablet only)
#endif

static lv_obj_t* g_settings_screen = nullptr;

static void back_btn_cb(lv_event_t* e) {
    lv_scr_load(g_main_screen);
}

static void wifi_setup_btn_cb(lv_event_t* e) {
    create_wifi_ui();
}

#ifdef __ANDROID__
static void printer_setup_btn_cb(lv_event_t* e) {
    (void)e;
    create_tablet_setup_ui();
}
static lv_obj_t* g_view_btn_label = nullptr;
static void view_toggle_cb(lv_event_t* e) {
    (void)e;
    g_screensaver_3d = !g_screensaver_3d;
    if (g_view_btn_label)
        lv_label_set_text(g_view_btn_label, g_screensaver_3d ? "Screensaver: 3D" : "Screensaver: 2D");
    save_settings();
    screensaver_view_changed();
}
static void scale_open_cb(lv_event_t* e) {
    (void)e;
    create_weigh_ui();
}
#endif

static void brightness_cb(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    pt_set_backlight((uint8_t)val, true);
    g_brightness = (uint8_t)val;
    save_settings();
}

static lv_obj_t* add_info_row(lv_obj_t* parent, const char* label, const String& value) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), PT_SZ(26));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* l = lv_label_create(row);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0x999999), 0);

    lv_obj_t* v = lv_label_create(row);
    lv_label_set_text(v, value.c_str());
    lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(v, lv_color_hex(0xFFFFFF), 0);

    return row;
}

// Only the first/last couple characters of a secret are shown, so the code
// isn't fully readable at a glance on a screen other people can walk past,
// while still being enough to tell "yep, that's the code I set."
static String mask_secret(const char* s) {
    size_t len = strlen(s);
    if (len == 0) return "-";
    if (len <= 4) return "****";
    String out = String(s).substring(0, 2) + "****" + String(s).substring(len - 2);
    return out;
}

void create_settings_ui() {
    if (g_settings_screen) {
        lv_obj_del(g_settings_screen);
        g_settings_screen = nullptr;
    }

    g_settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_settings_screen, lv_color_hex(0x101418), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(g_settings_screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_center(root);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, PT_SZ(12), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, PT_SZ(5), LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    // NOT scrollable: on this display, a scrollable column combined with the
    // partial (top/bottom split) render buffer produced a torn frame - the
    // slider showed as two mismatched halves and touch input stopped
    // registering (likely a scroll-triggered re-layout landing between the
    // two partial buffer flushes). Content is now sized to fit within 480px
    // without scrolling instead - see the trimmed row heights below.
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(root);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    add_info_row(root, "Firmware", FILATRACK_VERSION);
#ifndef __ANDROID__
    // The tablet's own WiFi/IP is managed by Android, not by this UI.
    add_info_row(root, "WiFi network", String(g_wifi_ssid));
    add_info_row(root, "Device IP", g_ip_addr);
#endif
    add_info_row(root, "Printer IP", String(g_printer_ip));
    add_info_row(root, "Printer serial", String(g_printer_serial));
    add_info_row(root, "Access code", mask_secret(g_printer_access_code));
    add_info_row(root, "Printer connection", bambu_is_connected() ? "Connected" : "Offline");

    lv_obj_t* hint = lv_label_create(root);
#ifdef __ANDROID__
    // No on-device webserver on the tablet build - settings come from a file.
    lv_label_set_text(hint, "To change printer settings, edit /sdcard/filatrack.conf and restart the app");
#else
    lv_label_set_text_fmt(hint, "To change printer settings, open http://%s/ in a browser", g_ip_addr.c_str());
#endif
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x777777), 0);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, lv_pct(100));

#ifndef __ANDROID__
    // WiFi setup is only relevant on the ESP32; the tablet uses Android's WiFi.
    lv_obj_t* wifi_btn = lv_btn_create(root);
    lv_obj_set_size(wifi_btn, lv_pct(100), PT_SZ(44));
    lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_t* wifi_btn_label = lv_label_create(wifi_btn);
    lv_label_set_text(wifi_btn_label, "WiFi Setup");
    lv_obj_set_style_text_font(wifi_btn_label, &lv_font_montserrat_14, 0);
    lv_obj_center(wifi_btn_label);
    lv_obj_add_event_cb(wifi_btn, wifi_setup_btn_cb, LV_EVENT_CLICKED, NULL);
#endif

#ifdef __ANDROID__
    // The tablet has no webserver, so printer IP/serial/access code are entered
    // on-screen here (see ui_tablet_setup.cpp).
    lv_obj_t* setup_btn = lv_btn_create(root);
    lv_obj_set_size(setup_btn, lv_pct(100), PT_SZ(44));
    lv_obj_set_style_bg_color(setup_btn, lv_color_hex(0x3465a4), LV_PART_MAIN);
    lv_obj_t* setup_btn_label = lv_label_create(setup_btn);
    lv_label_set_text(setup_btn_label, "Printer setup");
    lv_obj_set_style_text_font(setup_btn_label, &lv_font_montserrat_14, 0);
    lv_obj_center(setup_btn_label);
    lv_obj_add_event_cb(setup_btn, printer_setup_btn_cb, LV_EVENT_CLICKED, NULL);

    // Row: screensaver 2D/3D toggle + open the FilaTrack Scale (scale) screen.
    lv_obj_t* row2 = lv_obj_create(root);
    lv_obj_set_size(row2, lv_pct(100), PT_SZ(44));
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row2, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row2, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(row2, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* view_btn = lv_btn_create(row2);
    lv_obj_set_flex_grow(view_btn, 1);
    lv_obj_set_height(view_btn, lv_pct(100));
    lv_obj_set_style_bg_color(view_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    g_view_btn_label = lv_label_create(view_btn);
    lv_label_set_text(g_view_btn_label, g_screensaver_3d ? "Screensaver: 3D" : "Screensaver: 2D");
    lv_obj_set_style_text_font(g_view_btn_label, &lv_font_montserrat_14, 0);
    lv_obj_center(g_view_btn_label);
    lv_obj_add_event_cb(view_btn, view_toggle_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* scale_btn = lv_btn_create(row2);
    lv_obj_set_flex_grow(scale_btn, 1);
    lv_obj_set_height(scale_btn, lv_pct(100));
    lv_obj_set_style_bg_color(scale_btn, lv_color_hex(0x8e44ad), LV_PART_MAIN);
    lv_obj_t* scale_lbl = lv_label_create(scale_btn);
    lv_label_set_text(scale_lbl, "Schaal beheren");
    lv_obj_set_style_text_font(scale_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(scale_lbl);
    lv_obj_add_event_cb(scale_btn, scale_open_cb, LV_EVENT_CLICKED, NULL);
#endif

    lv_obj_t* bright_row = lv_obj_create(root);
    lv_obj_set_size(bright_row, lv_pct(100), PT_SZ(36));
    lv_obj_set_style_bg_opa(bright_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bright_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bright_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(bright_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bright_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(bright_row, PT_SZ(10), LV_PART_MAIN);
    lv_obj_clear_flag(bright_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bl = lv_label_create(bright_row);
    lv_label_set_text(bl, "Brightness");
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bl, lv_color_hex(0x999999), 0);

    lv_obj_t* slider = lv_slider_create(bright_row);
    lv_obj_set_flex_grow(slider, 1);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, g_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, brightness_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* back_btn = lv_btn_create(root);
    lv_obj_set_size(back_btn, lv_pct(100), PT_SZ(46));
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3465a4), LV_PART_MAIN);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_18, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_scr_load(g_settings_screen);
}
