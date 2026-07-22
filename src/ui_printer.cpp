#include "ui_printer.h"
#include "lang.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include "storage.h"
#include "bambu_mqtt.h"
#include "ui_settings.h"
#include "ui_filament.h"
#include "ui_files.h"
#include "ui_move.h"
#ifdef __ANDROID__
#include "ui_weigh.h"    // "Scale" screen (tablet only)
#include "ui_spools.h"   // "Spools" screen (tablet only)
#include "filament_track.h"  // filament_remaining() for the per-slot grams (tablet only)
#endif
#include "pt/pt_display.h"
#include "ui_scale.h"   // tablet font scaling (Android only)

lv_obj_t* g_main_screen = nullptr;
volatile int g_pending_action = ACT_NONE;
volatile int g_pending_speed_level = 0;

static lv_obj_t* g_wifi_label = nullptr;
static lv_obj_t* g_conn_dot = nullptr;
static lv_obj_t* g_conn_label = nullptr;
static lv_obj_t* g_clock_label = nullptr;
static lv_obj_t* g_warn_banner = nullptr;
static lv_obj_t* g_warn_label = nullptr;
static lv_obj_t* g_dry_banner = nullptr;   // "dry the silica gel" reminder + button
static lv_obj_t* g_dry_label = nullptr;
static lv_obj_t* g_state_label = nullptr;
static lv_obj_t* g_task_label = nullptr;
static lv_obj_t* g_bar = nullptr;
static lv_obj_t* g_pct_label = nullptr;
static lv_obj_t* g_remain_label = nullptr;
static lv_obj_t* g_nozzle_label = nullptr;
static lv_obj_t* g_bed_label = nullptr;
static lv_obj_t* g_chamber_label = nullptr;
static lv_obj_t* g_ams_row = nullptr;
static lv_obj_t* g_ams_boxes[AMS_MAX_UNITS] = {nullptr};
static lv_obj_t* g_ams_tray_dots[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{nullptr}};
static lv_obj_t* g_ams_tray_labels[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{nullptr}};
static lv_obj_t* g_ams_tray_grams[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{nullptr}};
static lv_obj_t* g_ams_tray_bars[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{nullptr}};
static lv_obj_t* g_ams_humidity_labels[AMS_MAX_UNITS] = {nullptr};
static lv_obj_t* g_ext_row = nullptr;
static lv_obj_t* g_ext_swatch = nullptr;
static lv_obj_t* g_ext_type_label = nullptr;
static lv_obj_t* g_pause_btn = nullptr;
static lv_obj_t* g_pause_btn_label = nullptr;
static lv_obj_t* g_stop_btn = nullptr;
static lv_obj_t* g_light_btn = nullptr;
static lv_obj_t* g_light_btn_label = nullptr;
static lv_obj_t* g_fan_btn = nullptr;
static lv_obj_t* g_fan_btn_label = nullptr;
static lv_obj_t* g_speed_dd = nullptr;   // speed level select (dropdown)
static lv_obj_t* g_slider = nullptr;

// --- Stop confirmation ---------------------------------------------------
// Stop aborts the running print irreversibly, so unlike the other controls it
// asks for confirmation first. Built from plain objects on the top layer (the
// lv_msgbox widget is disabled in lv_conf.h): a full-screen dimmed backdrop
// makes it modal (swallows taps outside the box), and only "Stoppen" queues
// the actual ACT_STOP.
static lv_obj_t* g_stop_modal = nullptr;

static void stop_confirm_close() {
    if (g_stop_modal) { lv_obj_del(g_stop_modal); g_stop_modal = nullptr; }
}

static void stop_confirm_yes_cb(lv_event_t* e) {
    g_pending_action = ACT_STOP;
    stop_confirm_close();
}

static void stop_confirm_no_cb(lv_event_t* e) {
    stop_confirm_close();
}

static void show_stop_confirm() {
    if (g_stop_modal) return; // already open

    // Dimmed full-screen backdrop on the top layer -> modal.
    lv_obj_t* bg = lv_obj_create(lv_layer_top());
    lv_obj_set_size(bg, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_50, 0);
    lv_obj_set_style_border_width(bg, 0, 0);
    lv_obj_set_style_radius(bg, 0, 0);
    lv_obj_set_style_pad_all(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    g_stop_modal = bg;

    // Dialog box.
    lv_obj_t* box = lv_obj_create(bg);
    lv_obj_set_size(box, 460, 230);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x1c1c1c), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x333333), 0);
    lv_obj_set_style_pad_all(box, 20, 0);
    lv_obj_set_style_pad_gap(box, 14, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(box);
    lv_label_set_text(title, T("dash.stop_confirm_t"));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t* txt = lv_label_create(box);
    lv_label_set_text(txt, T("dash.stop_confirm_b"));
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(txt, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_width(txt, lv_pct(100));
    lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);

    // Button row.
    lv_obj_t* row = lv_obj_create(box);
    lv_obj_set_size(row, lv_pct(100), 60);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 10, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* cancel = lv_btn_create(row);
    lv_obj_set_flex_grow(cancel, 1);
    lv_obj_set_height(cancel, 56);
    lv_obj_set_style_bg_color(cancel, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_t* cl = lv_label_create(cancel);
    lv_label_set_text(cl, T("cancel"));
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_18, 0);
    lv_obj_center(cl);
    lv_obj_add_event_cb(cancel, stop_confirm_no_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* confirm = lv_btn_create(row);
    lv_obj_set_flex_grow(confirm, 1);
    lv_obj_set_height(confirm, 56);
    lv_obj_set_style_bg_color(confirm, lv_color_hex(0xa40000), LV_PART_MAIN);
    lv_obj_t* cf = lv_label_create(confirm);
    lv_label_set_text(cf, T("dash.stop_yes"));
    lv_obj_set_style_text_font(cf, &lv_font_montserrat_18, 0);
    lv_obj_center(cf);
    lv_obj_add_event_cb(confirm, stop_confirm_yes_cb, LV_EVENT_CLICKED, NULL);
}

static void act_cb(lv_event_t* e) {
    int action = (int)(uintptr_t)lv_event_get_user_data(e);
    // Stop is destructive - confirm before sending it instead of firing on tap.
    if (action == ACT_STOP) {
        show_stop_confirm();
        return;
    }
    // Queue only - MQTT publish happens from the main loop, after this frame
    // has already been flushed, so we don't block lv_task_handler() mid-redraw.
    g_pending_action = action;
}

static void slider_event_cb(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    pt_set_backlight((uint8_t)val, true);
    g_brightness = (uint8_t)val;
    save_settings();
}

static void settings_btn_cb(lv_event_t* e) {
    create_settings_ui();
}

static void files_btn_cb(lv_event_t* e) {
    create_files_ui();
}

static void move_btn_cb(lv_event_t* e) {
    create_move_ui();
}

#ifdef __ANDROID__
static void spools_btn_cb(lv_event_t* e) {
    create_spools_ui();
}
static void ams_cell_cb(lv_event_t* e) {   // tap an AMS/external slot -> pick a roll
    filament_open_roll_picker((int)(uintptr_t)lv_event_get_user_data(e));
}
#endif

static lv_obj_t* make_row(lv_obj_t* parent, int32_t height) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), PT_SZ(height));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

static lv_obj_t* make_btn(lv_obj_t* parent, const char* text, uint32_t color, int action, lv_obj_t** label_out) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, PT_SZ(64));
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)action);
    if (label_out) *label_out = label;
    return btn;
}

// Speed level picked from the dropdown -> queued, applied in the main loop.
static void speed_dd_cb(lv_event_t* e) {
    lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
    g_pending_speed_level = (int)lv_dropdown_get_selected(dd) + 1;  // 1..4
}

#ifdef __ANDROID__
// "Dried it now" on the silica-gel reminder banner: restart the timer + re-arm.
// Runs on the LVGL/main thread, so it can touch storage directly.
static void dry_done_cb(lv_event_t*) {
    g_dry_last_dried = (long)time(nullptr);
    g_dry_notified = false;
    g_dry_hum_ack = true;    // mute the humidity alarm until the AMS reads dry again
    save_settings();
    if (g_dry_banner) lv_obj_add_flag(g_dry_banner, LV_OBJ_FLAG_HIDDEN);
}
#endif

void create_printer_ui() {
    lv_obj_clean(g_main_screen);
    lv_obj_set_style_bg_color(g_main_screen, lv_color_hex(g_bg_color), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(g_main_screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_center(root);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, PT_SZ(12), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, PT_SZ(10), LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Header: wifi/ip (left) + printer connection (right) ---
    lv_obj_t* header = make_row(root, 30);
#ifdef __ANDROID__
    // The tablet OS owns the WiFi, so instead of an IP we show a clock here.
    g_clock_label = lv_label_create(header);
    lv_obj_set_style_text_font(g_clock_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_clock_label, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(g_clock_label, "");
#endif
#ifndef __ANDROID__
    // On the ESP32 this shows the device's own WiFi IP. On the Android tablet the
    // OS owns the WiFi, so the label is dropped (g_wifi_label stays null and
    // update_status_label() skips it).
    g_wifi_label = lv_label_create(header);
    lv_obj_set_style_text_font(g_wifi_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_wifi_label, lv_color_hex(0xAAAAAA), 0);
#endif

    // Connection status: a colored dot + text, kept together in a sub-box so the
    // header's SPACE_BETWEEN doesn't split them.
    lv_obj_t* conn_box = lv_obj_create(header);
    lv_obj_set_size(conn_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(conn_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(conn_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(conn_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(conn_box, PT_SZ(7), LV_PART_MAIN);
    lv_obj_set_flex_flow(conn_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(conn_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(conn_box, LV_OBJ_FLAG_SCROLLABLE);

    g_conn_dot = lv_obj_create(conn_box);
    lv_obj_set_size(g_conn_dot, PT_SZ(14), PT_SZ(14));
    lv_obj_set_style_radius(g_conn_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_conn_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_conn_dot, lv_color_hex(0xe74c3c), LV_PART_MAIN);
    lv_obj_clear_flag(g_conn_dot, LV_OBJ_FLAG_SCROLLABLE);

    g_conn_label = lv_label_create(conn_box);
    lv_obj_set_style_text_font(g_conn_label, &lv_font_montserrat_14, 0);

    lv_obj_t* move_btn = lv_btn_create(header);
    lv_obj_set_size(move_btn, PT_SZ(74), PT_SZ(26));
    lv_obj_set_style_bg_color(move_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* move_label = lv_label_create(move_btn);
    lv_label_set_text(move_label, T("nav.move"));
    lv_obj_set_style_text_font(move_label, &lv_font_montserrat_12, 0);
    lv_obj_center(move_label);
    lv_obj_add_event_cb(move_btn, move_btn_cb, LV_EVENT_CLICKED, NULL);

#ifdef __ANDROID__
    lv_obj_t* spools_btn = lv_btn_create(header);
    lv_obj_set_size(spools_btn, PT_SZ(78), PT_SZ(26));
    lv_obj_set_style_bg_color(spools_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* spools_label = lv_label_create(spools_btn);
    lv_label_set_text(spools_label, T("nav.spools"));
    lv_obj_set_style_text_font(spools_label, &lv_font_montserrat_12, 0);
    lv_obj_center(spools_label);
    lv_obj_add_event_cb(spools_btn, spools_btn_cb, LV_EVENT_CLICKED, NULL);
#endif

    lv_obj_t* files_btn = lv_btn_create(header);
    lv_obj_set_size(files_btn, PT_SZ(80), PT_SZ(26));
    lv_obj_set_style_bg_color(files_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* files_label = lv_label_create(files_btn);
    lv_label_set_text(files_label, T("nav.files"));
    lv_obj_set_style_text_font(files_label, &lv_font_montserrat_12, 0);
    lv_obj_center(files_label);
    lv_obj_add_event_cb(files_btn, files_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* settings_btn = lv_btn_create(header);
    lv_obj_set_size(settings_btn, PT_SZ(90), PT_SZ(26));
    lv_obj_set_style_bg_color(settings_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* settings_label = lv_label_create(settings_btn);
    lv_label_set_text(settings_label, T("nav.settings"));
    lv_obj_set_style_text_font(settings_label, &lv_font_montserrat_12, 0);
    lv_obj_center(settings_label);
    lv_obj_add_event_cb(settings_btn, settings_btn_cb, LV_EVENT_CLICKED, NULL);

    // --- Print state + task name ---
    lv_obj_t* state_row = make_row(root, 50);
    lv_obj_set_flex_align(state_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(state_row, PT_SZ(10), LV_PART_MAIN);
    lv_obj_t* state_col = lv_obj_create(state_row);
    lv_obj_set_height(state_col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(state_col, 1);
    lv_obj_set_style_bg_opa(state_col, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(state_col, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(state_col, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(state_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(state_col, LV_OBJ_FLAG_SCROLLABLE);

    g_state_label = lv_label_create(state_col);
    lv_label_set_text(g_state_label, T("dash.connecting"));
    lv_obj_set_style_text_font(g_state_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_state_label, lv_color_hex(0xFFFFFF), 0);

    g_task_label = lv_label_create(state_col);
    lv_label_set_text(g_task_label, "");
    lv_obj_set_style_text_font(g_task_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_task_label, lv_color_hex(0x999999), 0);
    lv_label_set_long_mode(g_task_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_task_label, lv_pct(100));

#ifdef __ANDROID__
    // Filament warning pill. It sits in the free space at the RIGHT of the state
    // row (a normal flex child), so it never covers the header buttons and never
    // grows the layout - it just takes space that was empty anyway.
    g_warn_banner = lv_obj_create(state_row);
    lv_obj_set_size(g_warn_banner, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(g_warn_banner, lv_color_hex(0x7a2020), 0);
    lv_obj_set_style_border_width(g_warn_banner, 0, 0);
    lv_obj_set_style_radius(g_warn_banner, 8, 0);
    lv_obj_set_style_pad_hor(g_warn_banner, PT_SZ(12), 0);
    lv_obj_set_style_pad_ver(g_warn_banner, PT_SZ(5), 0);
    lv_obj_clear_flag(g_warn_banner, LV_OBJ_FLAG_SCROLLABLE);
    g_warn_label = lv_label_create(g_warn_banner);
    lv_obj_set_style_text_font(g_warn_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_warn_label, lv_color_hex(0xffe0e0), 0);
    lv_label_set_text(g_warn_label, "");
    lv_obj_add_flag(g_warn_banner, LV_OBJ_FLAG_HIDDEN);

    // Silica-gel drying reminder pill with a "dried it" button, shown when overdue.
    g_dry_banner = lv_obj_create(state_row);
    lv_obj_set_size(g_dry_banner, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(g_dry_banner, lv_color_hex(0x5a4a1e), 0);
    lv_obj_set_style_border_width(g_dry_banner, 0, 0);
    lv_obj_set_style_radius(g_dry_banner, 8, 0);
    lv_obj_set_style_pad_hor(g_dry_banner, PT_SZ(12), 0);
    lv_obj_set_style_pad_ver(g_dry_banner, PT_SZ(5), 0);
    lv_obj_set_style_pad_gap(g_dry_banner, PT_SZ(10), 0);
    lv_obj_set_flex_flow(g_dry_banner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_dry_banner, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_dry_banner, LV_OBJ_FLAG_SCROLLABLE);
    g_dry_label = lv_label_create(g_dry_banner);
    lv_obj_set_style_text_font(g_dry_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_dry_label, lv_color_hex(0xffe8b0), 0);
    lv_label_set_text(g_dry_label, "");
    lv_obj_t* dry_btn = lv_btn_create(g_dry_banner);
    lv_obj_set_style_bg_color(dry_btn, lv_color_hex(0x3a7d44), 0);
    lv_obj_set_style_pad_hor(dry_btn, PT_SZ(10), 0);
    lv_obj_set_style_pad_ver(dry_btn, PT_SZ(4), 0);
    lv_obj_t* dry_btn_lbl = lv_label_create(dry_btn);
    lv_label_set_text(dry_btn_lbl, T("set.dry_done"));
    lv_obj_set_style_text_font(dry_btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(dry_btn, dry_done_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(g_dry_banner, LV_OBJ_FLAG_HIDDEN);
#endif

    // --- Progress bar + % + remaining time ---
    lv_obj_t* prog_row = make_row(root, 34);
    g_bar = lv_bar_create(prog_row);
    lv_obj_set_flex_grow(g_bar, 1);
    lv_obj_set_height(g_bar, PT_SZ(18));
    lv_bar_set_range(g_bar, 0, 100);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x2ecc71), LV_PART_INDICATOR);

    g_pct_label = lv_label_create(prog_row);
    lv_label_set_text(g_pct_label, "0%");
    lv_obj_set_style_text_font(g_pct_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_pct_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_left(g_pct_label, 10, 0);

    g_remain_label = lv_label_create(prog_row);
    lv_label_set_text(g_remain_label, "");
    lv_obj_set_style_text_font(g_remain_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_remain_label, lv_color_hex(0x999999), 0);
    lv_obj_set_style_pad_left(g_remain_label, 10, 0);

    // --- Temperatures ---
    lv_obj_t* temp_row = make_row(root, 34);
    g_nozzle_label = lv_label_create(temp_row);
    g_bed_label = lv_label_create(temp_row);
    g_chamber_label = lv_label_create(temp_row);
    for (lv_obj_t* l : {g_nozzle_label, g_bed_label, g_chamber_label}) {
        lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    }

    // --- AMS units ---
    // Each unit box holds up to 4 tray cells side by side (color swatch +
    // material type text). Boxes don't wrap internally - if more than ~2
    // units are present the row itself scrolls horizontally instead.
    g_ams_row = lv_obj_create(root);
    lv_obj_set_size(g_ams_row, lv_pct(100), PT_SZ(94));
    lv_obj_set_style_bg_opa(g_ams_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_ams_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_ams_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(g_ams_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_ams_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(g_ams_row, 8, LV_PART_MAIN);
    lv_obj_set_scroll_dir(g_ams_row, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(g_ams_row, LV_SCROLLBAR_MODE_OFF);

    for (int u = 0; u < AMS_MAX_UNITS; u++) {
        lv_obj_t* box = lv_obj_create(g_ams_row);
        lv_obj_set_size(box, PT_SZ(280), PT_SZ(94));
        lv_obj_set_style_bg_color(box, lv_color_hex(0x1c1c1c), LV_PART_MAIN);
        lv_obj_set_style_border_color(box, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_pad_hor(box, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(box, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_gap(box, 2, LV_PART_MAIN);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        g_ams_boxes[u] = box;

        // header row: "AMS n" on the left, humidity on the right
        lv_obj_t* head = lv_obj_create(box);
        lv_obj_set_size(head, lv_pct(100), PT_SZ(16));
        lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(head, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(head, 0, LV_PART_MAIN);
        lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* unm = lv_label_create(head);
        lv_label_set_text_fmt(unm, "AMS %d", u + 1);
        lv_obj_set_style_text_font(unm, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(unm, lv_color_hex(0x999999), 0);
        lv_obj_align(unm, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_t* hum = lv_label_create(head);
        lv_label_set_text(hum, "");
        lv_obj_set_style_text_font(hum, &lv_font_montserrat_12, 0);
        lv_obj_align(hum, LV_ALIGN_RIGHT_MID, 0, 0);
        g_ams_humidity_labels[u] = hum;

        // tray row: the 4 slot cells side by side (each tappable -> roll picker)
        lv_obj_t* tray_row = lv_obj_create(box);
        lv_obj_set_size(tray_row, lv_pct(100), PT_SZ(68));
        lv_obj_set_style_bg_opa(tray_row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(tray_row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(tray_row, 0, LV_PART_MAIN);
        lv_obj_set_flex_flow(tray_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tray_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(tray_row, LV_OBJ_FLAG_SCROLLABLE);

        for (int t = 0; t < AMS_MAX_TRAYS; t++) {
            lv_obj_t* cell = lv_obj_create(tray_row);
            lv_obj_set_size(cell, PT_SZ(62), PT_SZ(68));
            lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(cell, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_gap(cell, 2, LV_PART_MAIN);
            lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
#ifdef __ANDROID__
            lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(cell, ams_cell_cb, LV_EVENT_CLICKED,
                                (void*)(uintptr_t)(u * AMS_MAX_TRAYS + t));
#endif
            lv_obj_t* dot = lv_obj_create(cell);
            lv_obj_set_size(dot, PT_SZ(50), PT_SZ(26));
            lv_obj_set_style_radius(dot, 6, LV_PART_MAIN);
            lv_obj_set_style_border_width(dot, 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(dot, lv_color_hex(0x555555), LV_PART_MAIN);
            lv_obj_set_style_bg_color(dot, lv_color_hex(0x333333), LV_PART_MAIN);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
            g_ams_tray_dots[u][t] = dot;

#ifdef __ANDROID__
            // Thin fullness bar (weighed roll only): green normally, red when low.
            lv_obj_t* fbar = lv_bar_create(cell);
            lv_obj_set_size(fbar, PT_SZ(50), PT_SZ(4));
            lv_bar_set_range(fbar, 0, 100);
            lv_bar_set_value(fbar, 0, LV_ANIM_OFF);
            lv_obj_set_style_radius(fbar, 2, LV_PART_MAIN);
            lv_obj_set_style_radius(fbar, 2, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(fbar, lv_color_hex(0x333333), LV_PART_MAIN);
            lv_obj_set_style_bg_color(fbar, lv_color_hex(0x2ecc71), LV_PART_INDICATOR);
            lv_obj_add_flag(fbar, LV_OBJ_FLAG_HIDDEN);
            g_ams_tray_bars[u][t] = fbar;
#endif

            lv_obj_t* type_label = lv_label_create(cell);
            lv_label_set_text(type_label, "-");
            lv_obj_set_style_text_font(type_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(type_label, lv_color_hex(0xAAAAAA), 0);
            g_ams_tray_labels[u][t] = type_label;

            lv_obj_t* grams = lv_label_create(cell);
            lv_label_set_text(grams, "");
            lv_obj_set_style_text_font(grams, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(grams, lv_color_hex(0x777777), 0);
            g_ams_tray_grams[u][t] = grams;
        }
        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
    }

    // --- External (manual) spool holder, outside the AMS ---
    g_ext_row = make_row(root, 40);
    // make_row() defaults to SPACE_BETWEEN (pushes children to opposite
    // ends) - override so the caption and swatch sit right next to each
    // other on the left instead of being spread across the full width.
    lv_obj_set_flex_align(g_ext_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(g_ext_row, 10, LV_PART_MAIN);
    lv_obj_t* ext_caption = lv_label_create(g_ext_row);
    lv_label_set_text(ext_caption, T("dash.external_spool"));
    lv_obj_set_style_text_font(ext_caption, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ext_caption, lv_color_hex(0x999999), 0);

    lv_obj_t* ext_inner = lv_obj_create(g_ext_row);
    lv_obj_set_size(ext_inner, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ext_inner, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ext_inner, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ext_inner, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(ext_inner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ext_inner, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(ext_inner, 8, LV_PART_MAIN);
    lv_obj_clear_flag(ext_inner, LV_OBJ_FLAG_SCROLLABLE);

    g_ext_swatch = lv_obj_create(ext_inner);
    lv_obj_set_size(g_ext_swatch, PT_SZ(40), PT_SZ(28));
    lv_obj_set_style_radius(g_ext_swatch, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_ext_swatch, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_ext_swatch, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ext_swatch, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_clear_flag(g_ext_swatch, LV_OBJ_FLAG_SCROLLABLE);
#ifdef __ANDROID__
    lv_obj_add_flag(g_ext_swatch, LV_OBJ_FLAG_CLICKABLE);   // tap -> pick a roll for the external spool
    lv_obj_add_event_cb(g_ext_swatch, ams_cell_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)254);
#endif

    g_ext_type_label = lv_label_create(ext_inner);
    lv_label_set_text(g_ext_type_label, "-");
    lv_obj_set_style_text_font(g_ext_type_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_ext_type_label, lv_color_hex(0xFFFFFF), 0);

    lv_obj_add_flag(g_ext_row, LV_OBJ_FLAG_HIDDEN);

    // --- Controls ---
    lv_obj_t* ctrl_row = make_row(root, 64);
    lv_obj_set_style_pad_gap(ctrl_row, 8, LV_PART_MAIN);
    g_pause_btn = make_btn(ctrl_row, "Pause", 0x3465a4, ACT_PAUSE_RESUME, &g_pause_btn_label);
    g_stop_btn = make_btn(ctrl_row, "Stop", 0xa40000, ACT_STOP, nullptr);
    g_light_btn = make_btn(ctrl_row, "Light", 0x555555, ACT_LIGHT_TOGGLE, &g_light_btn_label);
    g_fan_btn = make_btn(ctrl_row, "Fan", 0x555555, ACT_FAN_TOGGLE, &g_fan_btn_label);
    // Speed: a select dropdown (Silent / Standard / Sport / Ludicrous) rather
    // than a cycling button.
    g_speed_dd = lv_dropdown_create(ctrl_row);
    lv_obj_set_flex_grow(g_speed_dd, 1);
    lv_obj_set_height(g_speed_dd, PT_SZ(64));
    lv_dropdown_set_options_static(g_speed_dd, "Silent\nStandard\nSport\nLudicrous");
    lv_dropdown_set_selected(g_speed_dd, 1);   // Standard (level 2)
    lv_obj_set_style_bg_color(g_speed_dd, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_color(g_speed_dd, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_speed_dd, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_speed_dd, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(g_speed_dd, speed_dd_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // Style the opened list too (dark, readable, scaled font).
    lv_obj_t* speed_list = lv_dropdown_get_list(g_speed_dd);
    lv_obj_set_style_bg_color(speed_list, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_color(speed_list, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(speed_list, &lv_font_montserrat_14, 0);

    // --- Brightness ---
    lv_obj_t* bright_row = make_row(root, 36);
    lv_obj_t* bl = lv_label_create(bright_row);
    lv_label_set_text(bl, T("dash.brightness"));
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bl, lv_color_hex(0x999999), 0);
    lv_obj_set_style_pad_right(bl, 10, 0);
    g_slider = lv_slider_create(bright_row);
    lv_obj_set_flex_grow(g_slider, 1);
    lv_slider_set_range(g_slider, 10, 100);
    lv_slider_set_value(g_slider, g_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(g_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    update_status_label();
    update_printer_ui();
}

void update_status_label() {
    if (g_wifi_label) {
        String wtxt = "WiFi: " + g_ip_addr;
        lv_label_set_text(g_wifi_label, wtxt.c_str());
    }
    bool connected = bambu_is_connected();
    if (g_conn_dot)
        lv_obj_set_style_bg_color(g_conn_dot, lv_color_hex(connected ? 0x2ecc71 : 0xe74c3c), 0);
    if (g_conn_label) {
        if (connected) {
            lv_label_set_text(g_conn_label, T("dash.connected"));
            lv_obj_set_style_text_color(g_conn_label, lv_color_hex(0x2ecc71), 0);
        } else {
            lv_label_set_text(g_conn_label, T("dash.offline"));
            lv_obj_set_style_text_color(g_conn_label, lv_color_hex(0xe74c3c), 0);
        }
    }
}

// Bambu's gcode_state -> our key. The raw value is protocol, never shown as-is
// unless it is a state we don't know about.
static const char* friendly_state(const char* raw) {
    if (strcmp(raw, "RUNNING") == 0) return T("dash.printing");
    if (strcmp(raw, "PAUSE") == 0) return T("dash.paused");
    if (strcmp(raw, "FINISH") == 0) return T("dash.finished");
    if (strcmp(raw, "FAILED") == 0) return T("dash.failed");
    if (strcmp(raw, "PREPARE") == 0) return T("dash.preparing");
    if (strcmp(raw, "IDLE") == 0) return T("dash.idle");
    return raw[0] ? raw : T("unknown");
}

static const char* speed_name(int level) {
    switch (level) {
        case 1: return "Speed: Silent";
        case 3: return "Speed: Sport";
        case 4: return "Speed: Ludicrous";
        default: return "Speed: Standard";
    }
}

void update_printer_ui() {
    if (!g_main_screen || !g_state_label) return;

    update_status_label();

#ifdef __ANDROID__
    if (g_clock_label) {
        time_t now = time(nullptr);
        struct tm* lt = localtime(&now);
        if (lt) { char ct[8]; strftime(ct, sizeof(ct), "%H:%M", lt); lv_label_set_text(g_clock_label, ct); }
    }
    // Silica-gel drying reminder banner - independent of the printer, so it runs
    // here (before the "no data yet" early-out below).
    if (g_dry_banner && g_dry_label) {
        bool show = false; long left_days = 0;
        bool by_hum = dry_humidity_alarm();   // AMS reports the desiccant is wet
        if (g_dry_interval_days > 0 && g_dry_last_dried > 0) {
            long left_sec = g_dry_last_dried + (long)g_dry_interval_days * 86400L - (long)time(nullptr);
            left_days = left_sec >= 0 ? (left_sec + 86399) / 86400 : -((-left_sec) / 86400);
            // "Always" keeps it on the dashboard as a live countdown; otherwise it
            // only appears once due, or within the configured advance window.
            if (g_dry_banner_always || left_sec <= (long)g_dry_advance_days * 86400L) show = true;
        }
        if (by_hum) show = true;
        if (show) {
            if (by_hum)              lv_label_set_text_fmt(g_dry_label, "%s (%s)", T("dash.dry_warn"), T("dash.dry_wet"));
            else if (left_days > 0)  lv_label_set_text_fmt(g_dry_label, "%s (%s %ld d)", T("dash.dry_warn"), T("dash.dry_in"), left_days);
            else if (left_days == 0) lv_label_set_text_fmt(g_dry_label, "%s (%s)", T("dash.dry_warn"), T("dash.dry_today"));
            else                     lv_label_set_text_fmt(g_dry_label, "%s (%ld d %s)", T("dash.dry_warn"), -left_days, T("dash.dry_late"));
            lv_obj_clear_flag(g_dry_banner, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_dry_banner, LV_OBJ_FLAG_HIDDEN);
        }
    }
#endif

    PrinterStatus& s = g_printer_status;

    if (!s.have_data) {
        lv_label_set_text(g_state_label, bambu_is_connected() ? T("dash.waiting") : T("dash.connecting"));
        lv_label_set_text(g_task_label, "");
        return;
    }

    lv_label_set_text(g_state_label, friendly_state(s.gcode_state));
    uint32_t state_color = 0xFFFFFF;
    if (strcmp(s.gcode_state, "RUNNING") == 0) state_color = 0x2ecc71;
    else if (strcmp(s.gcode_state, "PAUSE") == 0) state_color = 0xf39c12;
    else if (strcmp(s.gcode_state, "FAILED") == 0) state_color = 0xe74c3c;
    else if (strcmp(s.gcode_state, "FINISH") == 0) state_color = 0x3498db;
    lv_obj_set_style_text_color(g_state_label, lv_color_hex(state_color), 0);

    lv_label_set_text(g_task_label, s.task_name[0] ? s.task_name : "-");

    lv_bar_set_value(g_bar, s.progress_pct, LV_ANIM_ON);
    lv_label_set_text_fmt(g_pct_label, "%d%%", s.progress_pct);
    {
        char rt[128] = ""; int rp = 0;
        if (s.remaining_min > 0) {
            rp += snprintf(rt + rp, sizeof(rt) - rp, T("dash.left_fmt"), s.remaining_min / 60, s.remaining_min % 60);
            time_t fin = time(nullptr) + (time_t)s.remaining_min * 60;
            struct tm* ft = localtime(&fin);
            if (ft) {
                char eta[8]; strftime(eta, sizeof(eta), "%H:%M", ft);
                rp += snprintf(rt + rp, sizeof(rt) - rp, "   %s %s", T("dash.eta"), eta);
            }
        }
        if (s.total_layers > 0)
            rp += snprintf(rt + rp, sizeof(rt) - rp, "%s%s %d/%d", rp ? "   " : "",
                           T("dash.layer"), s.layer_num, s.total_layers);
#ifdef __ANDROID__
        // Append the running print's live filament use + cost when known.
        float lg = 0, lc = 0;
        if (filament_live_cost(&lg, &lc)) {
            if (lc > 0) rp += snprintf(rt + rp, sizeof(rt) - rp, "%s%.0fg EUR%.2f", rp ? "   " : "", lg, lc);
            else        rp += snprintf(rt + rp, sizeof(rt) - rp, "%s%.0fg", rp ? "   " : "", lg);
        }
#endif
        lv_label_set_text(g_remain_label, rt);
    }

    // Bambu truncates temperatures (23.9 shows as 23), so we do too - keeps our
    // readout matching Bambu Studio/Handy instead of rounding a degree higher.
    lv_label_set_text_fmt(g_nozzle_label, "%s %d/%d\xC2\xB0" "C", T("dash.nozzle"), (int)s.nozzle_temp, (int)s.nozzle_target);
    lv_label_set_text_fmt(g_bed_label, "%s %d/%d\xC2\xB0" "C", T("dash.bed"), (int)s.bed_temp, (int)s.bed_target);
    // Only printers with a real chamber sensor (X1/H2) get a chamber reading; the
    // P1/A1 series report a bogus placeholder, so we leave it blank like Bambu does.
    if (printer_has_chamber_sensor())
        lv_label_set_text_fmt(g_chamber_label, "%s %d\xC2\xB0" "C", T("dash.chamber"), (int)s.chamber_temp);
    else
        lv_label_set_text(g_chamber_label, "");

    for (int u = 0; u < AMS_MAX_UNITS; u++) {
        AmsUnit& unit = s.ams[u];
        if (u >= s.ams_count || !unit.present) {
            lv_obj_add_flag(g_ams_boxes[u], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(g_ams_boxes[u], LV_OBJ_FLAG_HIDDEN);
        if (g_ams_humidity_labels[u]) {
            // Bambu humidity grade 1-5: 1 = most humid (bad) ... 5 = driest (good).
            int hu = unit.humidity;
            if (hu >= 1) {
                const char* hl; uint32_t hc;
                if (hu >= 4)      { hl = T("dash.hum_dry"); hc = 0x2ecc71; }
                else if (hu == 3) { hl = T("dash.hum_ok");  hc = 0xf39c12; }
                else              { hl = T("dash.hum_wet"); hc = 0xe74c3c; }
                // Show the actual RH% (what Bambu Studio shows) when reported, else the grade label.
                if (unit.humidity_raw >= 0)
                    lv_label_set_text_fmt(g_ams_humidity_labels[u], T("dash.humidity_pct"), unit.humidity_raw);
                else
                    lv_label_set_text_fmt(g_ams_humidity_labels[u], T("dash.humidity_fmt"), hl);
                lv_obj_set_style_text_color(g_ams_humidity_labels[u], lv_color_hex(hc), 0);
            } else {
                lv_label_set_text(g_ams_humidity_labels[u], "");
            }
        }
        for (int t = 0; t < AMS_MAX_TRAYS; t++) {
            AmsTraySlot& slot = unit.trays[t];
            lv_obj_t* dot = g_ams_tray_dots[u][t];
            lv_obj_t* label = g_ams_tray_labels[u][t];
            if (slot.present) {
                lv_obj_set_style_bg_color(dot, lv_color_hex(slot.color), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
                if (label) {
                    lv_label_set_text(label, slot.type);
                    lv_obj_set_style_text_color(label, lv_color_hex(0xAAAAAA), 0);
                }
            } else {
                lv_obj_set_style_bg_color(dot, lv_color_hex(0x333333), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(dot, LV_OPA_50, LV_PART_MAIN);
                if (label) {
                    lv_label_set_text(label, "-");
                    lv_obj_set_style_text_color(label, lv_color_hex(0x555555), 0);
                }
            }
            // Highlight the slot that is currently feeding the nozzle.
            bool act = (u * AMS_MAX_TRAYS + t) == s.active_tray_now;
            lv_obj_set_style_border_width(dot, act ? 3 : 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(dot, lv_color_hex(act ? 0x2ecc71 : 0x555555), LV_PART_MAIN);
#ifdef __ANDROID__
            {
                int sl = u * AMS_MAX_TRAYS + t;
                float rem = filament_remaining(sl);
                float cap = filament_capacity(sl);
                if (g_ams_tray_grams[u][t]) {
                    if (rem >= 0) lv_label_set_text_fmt(g_ams_tray_grams[u][t], "%.0f g", rem);
                    else          lv_label_set_text(g_ams_tray_grams[u][t], "");
                }
                if (g_ams_tray_bars[u][t]) {
                    if (cap > 0 && rem >= 0) {
                        int pctv = (int)(rem / cap * 100.0f + 0.5f);
                        if (pctv < 0) pctv = 0; if (pctv > 100) pctv = 100;
                        lv_bar_set_value(g_ams_tray_bars[u][t], pctv, LV_ANIM_OFF);
                        lv_obj_set_style_bg_color(g_ams_tray_bars[u][t],
                            lv_color_hex(filament_slot_low(sl) ? 0xe74c3c : 0x2ecc71), LV_PART_INDICATOR);
                        lv_obj_clear_flag(g_ams_tray_bars[u][t], LV_OBJ_FLAG_HIDDEN);
                    } else {
                        lv_obj_add_flag(g_ams_tray_bars[u][t], LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }
#endif
        }
    }

    if (s.external_spool.present) {
        lv_obj_clear_flag(g_ext_row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(g_ext_swatch, lv_color_hex(s.external_spool.color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_ext_swatch, LV_OPA_COVER, LV_PART_MAIN);
        lv_label_set_text(g_ext_type_label, s.external_spool.type);
        bool eact = (s.active_tray_now == 254);
        lv_obj_set_style_border_width(g_ext_swatch, eact ? 3 : 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_ext_swatch, lv_color_hex(eact ? 0x2ecc71 : 0x555555), LV_PART_MAIN);
    } else {
        lv_obj_add_flag(g_ext_row, LV_OBJ_FLAG_HIDDEN);
    }

    bool is_paused = (strcmp(s.gcode_state, "PAUSE") == 0);
    bool is_running = (strcmp(s.gcode_state, "RUNNING") == 0);
    if (g_pause_btn_label) lv_label_set_text(g_pause_btn_label, is_paused ? T("dash.resume") : T("dash.pause"));
    lv_obj_set_style_bg_color(g_pause_btn, lv_color_hex(is_paused ? 0x2ecc71 : 0x3465a4), LV_PART_MAIN);
    // Pause/resume/stop only make sense while a job is active.
    if (is_running || is_paused) {
        lv_obj_clear_state(g_pause_btn, LV_STATE_DISABLED);
        lv_obj_clear_state(g_stop_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(g_pause_btn, LV_STATE_DISABLED);
        lv_obj_add_state(g_stop_btn, LV_STATE_DISABLED);
    }

    if (g_light_btn_label) lv_label_set_text(g_light_btn_label, s.light_on ? T("dash.light_on") : T("dash.light_off"));
    lv_obj_set_style_bg_color(g_light_btn, lv_color_hex(s.light_on ? 0xf1c40f : 0x555555), LV_PART_MAIN);

    if (g_fan_btn_label) lv_label_set_text_fmt(g_fan_btn_label, T("dash.fan_pct"), s.fan_speed_pct);

    // Reflect the printer's current speed in the dropdown, but don't fight the
    // user while they have the list open.
    if (g_speed_dd && !lv_dropdown_is_open(g_speed_dd)) {
        int sel = s.speed_level - 1;
        if (sel < 0) sel = 0;
        if (sel > 3) sel = 3;
        if ((int)lv_dropdown_get_selected(g_speed_dd) != sel) lv_dropdown_set_selected(g_speed_dd, sel);
    }

#ifdef __ANDROID__
    if (g_warn_banner && g_warn_label) {
        float sh = filament_shortfall();
        if (sh > 0) {
            lv_label_set_text_fmt(g_warn_label, T("dash.short_fmt"), sh);
            lv_obj_clear_flag(g_warn_banner, LV_OBJ_FLAG_HIDDEN);
        } else if (filament_any_low()) {
            lv_label_set_text(g_warn_label, T("dash.warn_low"));
            lv_obj_clear_flag(g_warn_banner, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_warn_banner, LV_OBJ_FLAG_HIDDEN);
        }
    }
#endif
}

