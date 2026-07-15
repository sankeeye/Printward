#include "ui_wifi.h"
#include "lang.h"
#include "ui_printer.h"
#include "ui_settings.h"
#include "storage.h"
#include <lvgl.h>
#include <WiFi.h>
#include "ui_scale.h"   // tablet font scaling (Android only)

#define WIFI_SCAN_MAX_RESULTS 12

static lv_obj_t* g_wifi_screen = nullptr;
static lv_obj_t* g_scan_btn = nullptr;
static lv_obj_t* g_scan_status_label = nullptr;
static lv_obj_t* g_wifi_list = nullptr;
static lv_obj_t* g_selected_label = nullptr;
static lv_obj_t* g_pass_ta = nullptr;
static lv_obj_t* g_keyboard = nullptr;
static lv_obj_t* g_connect_msg_label = nullptr;

static char g_selected_ssid[32] = "";
static bool g_scanning = false;

// Set from connect_btn_cb() (an LVGL click callback) and drained from
// wifi_ui_loop() (called from the main loop, after this frame has already
// been flushed to the display) - WiFi.disconnect()/WiFi.begin() and the NVS
// write in save_settings() can each block for tens to hundreds of ms, and
// doing that inside the click callback stalled lv_task_handler() mid-redraw,
// producing a torn/split frame on this display (same class of bug as the
// original BLE/MQTT tearing issue - see act_cb() in ui_printer.cpp for the
// same pattern applied there).
static volatile bool g_connect_requested = false;
static bool g_connecting = false;
static unsigned long g_connect_started_ms = 0;

static char g_scan_ssids[WIFI_SCAN_MAX_RESULTS][33];
static int g_scan_count = 0;

static void back_btn_cb(lv_event_t* e) {
    create_settings_ui();
}

static void select_ssid(const char* ssid) {
    strncpy(g_selected_ssid, ssid, 31);
    g_selected_ssid[31] = '\0';
    lv_label_set_text_fmt(g_selected_label, T("wifi.selected"), g_selected_ssid);
    // Not auto-focusing the password field here: this UI has no keyboard/
    // group-based input, only the touchscreen, and LVGL's FOCUSED event
    // (which shows the on-screen keyboard, see pass_ta_event_cb) is driven
    // by that group-focus system - forcing LV_STATE_FOCUSED directly doesn't
    // trigger it. Simplest reliable UX: the user taps the password field
    // themselves, which fires a plain CLICKED event instead.
}

static void list_item_cb(lv_event_t* e) {
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= g_scan_count) return;
    select_ssid(g_scan_ssids[idx]);
}

static void scan_btn_cb(lv_event_t* e) {
    if (g_scanning) return;
    lv_obj_clean(g_wifi_list);
    lv_label_set_text(g_scan_status_label, T("wifi.scanning"));
    // Async - returns immediately, doesn't block lv_task_handler(). Actual
    // results are picked up by wifi_ui_loop() polling WiFi.scanComplete().
    WiFi.scanNetworks(true);
    g_scanning = true;
}

// Bound to LV_EVENT_CLICKED rather than FOCUSED/DEFOCUSED: this screen has
// no keyboard-input-device/group set up (touch only), and LVGL only fires
// FOCUSED via that group-focus mechanism - a plain tap only ever produces
// CLICKED, so that's what actually shows the on-screen keyboard here.
static void pass_ta_event_cb(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    lv_keyboard_set_textarea(g_keyboard, ta);
    lv_obj_clear_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_keyboard);
}

static void keyboard_event_cb(lv_event_t* e) {
    lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(g_keyboard, NULL);
    lv_obj_clear_state(g_pass_ta, LV_STATE_FOCUSED);
}

static void connect_btn_cb(lv_event_t* e) {
    if (g_selected_ssid[0] == '\0') {
        lv_label_set_text(g_connect_msg_label, T("wifi.pick_first"));
        return;
    }

    strncpy(g_wifi_ssid, g_selected_ssid, 31);
    g_wifi_ssid[31] = '\0';
    strncpy(g_wifi_pass, lv_textarea_get_text(g_pass_ta), 63);
    g_wifi_pass[63] = '\0';

    // Don't touch WiFi or flash here - just queue it. See g_connect_requested
    // comment: WiFi.disconnect()/begin() and save_settings() are slow enough
    // to tear the frame if run straight from this click callback.
    g_connect_requested = true;
    lv_label_set_text(g_connect_msg_label, T("dash.connecting"));
}

void create_wifi_ui() {
    if (g_wifi_screen) {
        lv_obj_del(g_wifi_screen);
        g_wifi_screen = nullptr;
    }

    g_scanning = false;
    g_selected_ssid[0] = '\0';
    g_scan_count = 0;

    g_wifi_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_wifi_screen, lv_color_hex(0x101418), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(g_wifi_screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Header ---
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_set_size(header, lv_pct(100), 34);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, T("set.wifi_setup"));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t* back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, T("back"));
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    // --- Scan row ---
    lv_obj_t* scan_row = lv_obj_create(root);
    lv_obj_set_size(scan_row, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(scan_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(scan_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scan_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(scan_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scan_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(scan_row, 12, LV_PART_MAIN);
    lv_obj_clear_flag(scan_row, LV_OBJ_FLAG_SCROLLABLE);

    g_scan_btn = lv_btn_create(scan_row);
    lv_obj_set_size(g_scan_btn, 160, 40);
    lv_obj_set_style_bg_color(g_scan_btn, lv_color_hex(0x3465a4), LV_PART_MAIN);
    lv_obj_t* scan_label = lv_label_create(g_scan_btn);
    lv_label_set_text(scan_label, T("wifi.scan"));
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_14, 0);
    lv_obj_center(scan_label);
    lv_obj_add_event_cb(g_scan_btn, scan_btn_cb, LV_EVENT_CLICKED, NULL);

    g_scan_status_label = lv_label_create(scan_row);
    lv_label_set_text(g_scan_status_label, "");
    lv_obj_set_style_text_font(g_scan_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_scan_status_label, lv_color_hex(0x999999), 0);

    g_selected_label = lv_label_create(root);
    lv_label_set_text(g_selected_label, T("wifi.selected_none"));
    lv_obj_set_style_text_font(g_selected_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_selected_label, lv_color_hex(0xFFFFFF), 0);

    // --- Password + connect ---
    // Placed above the results list (not below) so that when the on-screen
    // keyboard pops up - anchored to the bottom of the screen - it covers
    // the list instead of hiding the very field you're typing into.
    g_pass_ta = lv_textarea_create(root);
    lv_obj_set_size(g_pass_ta, lv_pct(100), 40);
    lv_textarea_set_password_mode(g_pass_ta, true);
    lv_textarea_set_one_line(g_pass_ta, true);
    lv_textarea_set_placeholder_text(g_pass_ta, T("scale.password"));
    lv_obj_add_event_cb(g_pass_ta, pass_ta_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* connect_row = lv_obj_create(root);
    lv_obj_set_size(connect_row, lv_pct(100), 50);
    lv_obj_set_style_bg_opa(connect_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(connect_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(connect_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(connect_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(connect_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(connect_row, 12, LV_PART_MAIN);
    lv_obj_clear_flag(connect_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* connect_btn = lv_btn_create(connect_row);
    lv_obj_set_size(connect_btn, 160, 44);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x2ecc71), LV_PART_MAIN);
    lv_obj_t* connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, T("wifi.connect_save"));
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_14, 0);
    lv_obj_center(connect_label);
    lv_obj_add_event_cb(connect_btn, connect_btn_cb, LV_EVENT_CLICKED, NULL);

    g_connect_msg_label = lv_label_create(connect_row);
    lv_label_set_text(g_connect_msg_label, "");
    lv_obj_set_style_text_font(g_connect_msg_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_connect_msg_label, lv_color_hex(0x999999), 0);

    // --- Results list (below password/connect - fine to have the keyboard
    // cover this part once you're done picking a network) ---
    g_wifi_list = lv_list_create(root);
    lv_obj_set_size(g_wifi_list, lv_pct(100), 110);
    lv_obj_set_style_bg_color(g_wifi_list, lv_color_hex(0x1c1c1c), LV_PART_MAIN);

    // --- On-screen keyboard, hidden until the password field is tapped ---
    g_keyboard = lv_keyboard_create(g_wifi_screen);
    lv_obj_set_size(g_keyboard, lv_pct(100), 200);
    lv_obj_align(g_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_keyboard, keyboard_event_cb, LV_EVENT_CANCEL, NULL);

    lv_scr_load(g_wifi_screen);
}

void wifi_ui_loop() {
    if (g_connect_requested) {
        g_connect_requested = false;
        save_settings();
        WiFi.disconnect();
        WiFi.begin(g_wifi_ssid, g_wifi_pass);
        // check_wifi_status() (already polled every loop from elsewhere)
        // will pick up the result and update the IP shown on header/settings.
        g_connecting = true;
        g_connect_started_ms = millis();
    }

    // The "Connecting..." message previously never changed - it only got set
    // once from connect_btn_cb and nothing ever updated it again, so it just
    // sat there forever whether the connection succeeded, failed, or was
    // still in progress. Poll WiFi.status() here instead so the screen
    // actually reflects what happened.
    if (g_connecting) {
        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            g_connecting = false;
            if (g_connect_msg_label) {
                lv_label_set_text_fmt(g_connect_msg_label, T("wifi.connected"), WiFi.localIP().toString().c_str());
            }
        } else if (millis() - g_connect_started_ms > 15000) {
            g_connecting = false;
            if (g_connect_msg_label) {
                lv_label_set_text(g_connect_msg_label, T("wifi.failed"));
            }
        }
    }

    if (!g_scanning) return;

    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;

    g_scanning = false;

    if (n == WIFI_SCAN_FAILED || n < 0) {
        lv_label_set_text(g_scan_status_label, T("wifi.scan_failed"));
        return;
    }

    // Dedupe by SSID (multiple APs/channels for the same network are common)
    // and keep the strongest signal seen for each.
    struct Found { char ssid[33]; int32_t rssi; };
    static Found found[WIFI_SCAN_MAX_RESULTS];
    int found_count = 0;

    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        int32_t rssi = WiFi.RSSI(i);

        int existing = -1;
        for (int j = 0; j < found_count; j++) {
            if (ssid.equals(found[j].ssid)) { existing = j; break; }
        }
        if (existing >= 0) {
            if (rssi > found[existing].rssi) found[existing].rssi = rssi;
        } else if (found_count < WIFI_SCAN_MAX_RESULTS) {
            strncpy(found[found_count].ssid, ssid.c_str(), 32);
            found[found_count].ssid[32] = '\0';
            found[found_count].rssi = rssi;
            found_count++;
        }
    }
    WiFi.scanDelete();

    // Sort strongest signal first (simple insertion sort - at most 12 items).
    for (int i = 1; i < found_count; i++) {
        Found key = found[i];
        int j = i - 1;
        while (j >= 0 && found[j].rssi < key.rssi) {
            found[j + 1] = found[j];
            j--;
        }
        found[j + 1] = key;
    }

    g_scan_count = found_count;
    lv_obj_clean(g_wifi_list);

    if (found_count == 0) {
        lv_label_set_text(g_scan_status_label, T("wifi.none_found"));
        return;
    }

    lv_label_set_text_fmt(g_scan_status_label, "%d network%s found", found_count, found_count == 1 ? "" : "s");

    for (int i = 0; i < found_count; i++) {
        strncpy(g_scan_ssids[i], found[i].ssid, 32);
        g_scan_ssids[i][32] = '\0';
        lv_obj_t* btn = lv_list_add_btn(g_wifi_list, LV_SYMBOL_WIFI, g_scan_ssids[i]);
        lv_obj_add_event_cb(btn, list_item_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }
}
