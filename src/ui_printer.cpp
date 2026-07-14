#include "ui_printer.h"
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
volatile bool g_ota_screen_requested = false;
volatile int g_ota_progress = -1;

static lv_obj_t* g_update_screen = nullptr;
static lv_obj_t* g_update_bar = nullptr;
static lv_obj_t* g_update_label = nullptr;
static lv_obj_t* g_update_pct_label = nullptr;

static lv_obj_t* g_wifi_label = nullptr;
static lv_obj_t* g_conn_label = nullptr;
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
    lv_label_set_text(title, "Print stoppen?");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t* txt = lv_label_create(box);
    lv_label_set_text(txt, "Weet je zeker dat je de print wilt afbreken?\n"
                           "Dit kan niet ongedaan worden gemaakt.");
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
    lv_label_set_text(cl, "Annuleren");
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_18, 0);
    lv_obj_center(cl);
    lv_obj_add_event_cb(cancel, stop_confirm_no_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* confirm = lv_btn_create(row);
    lv_obj_set_flex_grow(confirm, 1);
    lv_obj_set_height(confirm, 56);
    lv_obj_set_style_bg_color(confirm, lv_color_hex(0xa40000), LV_PART_MAIN);
    lv_obj_t* cf = lv_label_create(confirm);
    lv_label_set_text(cf, "Stoppen");
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
static void scale_btn_cb(lv_event_t* e) {
    create_weigh_ui();
}
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
#ifndef __ANDROID__
    // On the ESP32 this shows the device's own WiFi IP. On the Android tablet the
    // OS owns the WiFi, so the label is dropped (g_wifi_label stays null and
    // update_status_label() skips it).
    g_wifi_label = lv_label_create(header);
    lv_obj_set_style_text_font(g_wifi_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_wifi_label, lv_color_hex(0xAAAAAA), 0);
#endif

    g_conn_label = lv_label_create(header);
    lv_obj_set_style_text_font(g_conn_label, &lv_font_montserrat_14, 0);

    lv_obj_t* move_btn = lv_btn_create(header);
    lv_obj_set_size(move_btn, PT_SZ(74), PT_SZ(26));
    lv_obj_set_style_bg_color(move_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* move_label = lv_label_create(move_btn);
    lv_label_set_text(move_label, "Move");
    lv_obj_set_style_text_font(move_label, &lv_font_montserrat_12, 0);
    lv_obj_center(move_label);
    lv_obj_add_event_cb(move_btn, move_btn_cb, LV_EVENT_CLICKED, NULL);

#ifdef __ANDROID__
    lv_obj_t* scale_btn = lv_btn_create(header);
    lv_obj_set_size(scale_btn, PT_SZ(74), PT_SZ(26));
    lv_obj_set_style_bg_color(scale_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* scale_label = lv_label_create(scale_btn);
    lv_label_set_text(scale_label, "Scale");
    lv_obj_set_style_text_font(scale_label, &lv_font_montserrat_12, 0);
    lv_obj_center(scale_label);
    lv_obj_add_event_cb(scale_btn, scale_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* spools_btn = lv_btn_create(header);
    lv_obj_set_size(spools_btn, PT_SZ(78), PT_SZ(26));
    lv_obj_set_style_bg_color(spools_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* spools_label = lv_label_create(spools_btn);
    lv_label_set_text(spools_label, "Spools");
    lv_obj_set_style_text_font(spools_label, &lv_font_montserrat_12, 0);
    lv_obj_center(spools_label);
    lv_obj_add_event_cb(spools_btn, spools_btn_cb, LV_EVENT_CLICKED, NULL);
#endif

    lv_obj_t* files_btn = lv_btn_create(header);
    lv_obj_set_size(files_btn, PT_SZ(80), PT_SZ(26));
    lv_obj_set_style_bg_color(files_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* files_label = lv_label_create(files_btn);
    lv_label_set_text(files_label, "Files");
    lv_obj_set_style_text_font(files_label, &lv_font_montserrat_12, 0);
    lv_obj_center(files_label);
    lv_obj_add_event_cb(files_btn, files_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* settings_btn = lv_btn_create(header);
    lv_obj_set_size(settings_btn, PT_SZ(90), PT_SZ(26));
    lv_obj_set_style_bg_color(settings_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* settings_label = lv_label_create(settings_btn);
    lv_label_set_text(settings_label, "Settings");
    lv_obj_set_style_text_font(settings_label, &lv_font_montserrat_12, 0);
    lv_obj_center(settings_label);
    lv_obj_add_event_cb(settings_btn, settings_btn_cb, LV_EVENT_CLICKED, NULL);

    // --- Print state + task name ---
    lv_obj_t* state_row = make_row(root, 50);
    lv_obj_set_flex_align(state_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* state_col = lv_obj_create(state_row);
    lv_obj_set_size(state_col, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(state_col, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(state_col, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(state_col, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(state_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(state_col, LV_OBJ_FLAG_SCROLLABLE);

    g_state_label = lv_label_create(state_col);
    lv_label_set_text(g_state_label, "Connecting...");
    lv_obj_set_style_text_font(g_state_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_state_label, lv_color_hex(0xFFFFFF), 0);

    g_task_label = lv_label_create(state_col);
    lv_label_set_text(g_task_label, "");
    lv_obj_set_style_text_font(g_task_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_task_label, lv_color_hex(0x999999), 0);
    lv_label_set_long_mode(g_task_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_task_label, lv_pct(100));

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
            lv_obj_set_size(dot, PT_SZ(50), PT_SZ(30));
            lv_obj_set_style_radius(dot, 6, LV_PART_MAIN);
            lv_obj_set_style_border_width(dot, 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(dot, lv_color_hex(0x555555), LV_PART_MAIN);
            lv_obj_set_style_bg_color(dot, lv_color_hex(0x333333), LV_PART_MAIN);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
            g_ams_tray_dots[u][t] = dot;

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
    lv_label_set_text(ext_caption, "External spool:");
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
    lv_label_set_text(bl, "Brightness");
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
    if (g_conn_label) {
        if (bambu_is_connected()) {
            lv_label_set_text(g_conn_label, "Printer: connected");
            lv_obj_set_style_text_color(g_conn_label, lv_color_hex(0x2ecc71), 0);
        } else {
            lv_label_set_text(g_conn_label, "Printer: offline");
            lv_obj_set_style_text_color(g_conn_label, lv_color_hex(0xe74c3c), 0);
        }
    }
}

static const char* friendly_state(const char* raw) {
    if (strcmp(raw, "RUNNING") == 0) return "Printing";
    if (strcmp(raw, "PAUSE") == 0) return "Paused";
    if (strcmp(raw, "FINISH") == 0) return "Finished";
    if (strcmp(raw, "FAILED") == 0) return "Failed";
    if (strcmp(raw, "PREPARE") == 0) return "Preparing";
    if (strcmp(raw, "IDLE") == 0) return "Idle";
    return raw[0] ? raw : "Unknown";
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

    PrinterStatus& s = g_printer_status;

    if (!s.have_data) {
        lv_label_set_text(g_state_label, bambu_is_connected() ? "Waiting for data..." : "Connecting...");
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
    if (s.remaining_min > 0) {
        lv_label_set_text_fmt(g_remain_label, "%dh%02dm left", s.remaining_min / 60, s.remaining_min % 60);
    } else {
        lv_label_set_text(g_remain_label, "");
    }

    lv_label_set_text_fmt(g_nozzle_label, "Nozzle %.0f/%.0f\xC2\xB0" "C", s.nozzle_temp, s.nozzle_target);
    lv_label_set_text_fmt(g_bed_label, "Bed %.0f/%.0f\xC2\xB0" "C", s.bed_temp, s.bed_target);
    lv_label_set_text_fmt(g_chamber_label, "Chamber %.0f\xC2\xB0" "C", s.chamber_temp);

    for (int u = 0; u < AMS_MAX_UNITS; u++) {
        AmsUnit& unit = s.ams[u];
        if (u >= s.ams_count || !unit.present) {
            lv_obj_add_flag(g_ams_boxes[u], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(g_ams_boxes[u], LV_OBJ_FLAG_HIDDEN);
        if (g_ams_humidity_labels[u]) {
            // Bambu humidity grade 1-5: 1 = dry (good) ... 5 = wet (bad).
            int hu = unit.humidity;
            if (hu >= 1) {
                const char* hl; uint32_t hc;
                if (hu <= 2)      { hl = "droog";    hc = 0x2ecc71; }
                else if (hu == 3) { hl = "redelijk"; hc = 0xf39c12; }
                else              { hl = "vochtig";  hc = 0xe74c3c; }
                lv_label_set_text_fmt(g_ams_humidity_labels[u], "Vocht: %s", hl);
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
#ifdef __ANDROID__
            if (g_ams_tray_grams[u][t]) {
                float rem = filament_remaining(u * AMS_MAX_TRAYS + t);
                if (rem >= 0) lv_label_set_text_fmt(g_ams_tray_grams[u][t], "%.0f g", rem);
                else          lv_label_set_text(g_ams_tray_grams[u][t], "");
            }
#endif
        }
    }

    if (s.external_spool.present) {
        lv_obj_clear_flag(g_ext_row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(g_ext_swatch, lv_color_hex(s.external_spool.color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(g_ext_swatch, LV_OPA_COVER, LV_PART_MAIN);
        lv_label_set_text(g_ext_type_label, s.external_spool.type);
    } else {
        lv_obj_add_flag(g_ext_row, LV_OBJ_FLAG_HIDDEN);
    }

    bool is_paused = (strcmp(s.gcode_state, "PAUSE") == 0);
    bool is_running = (strcmp(s.gcode_state, "RUNNING") == 0);
    if (g_pause_btn_label) lv_label_set_text(g_pause_btn_label, is_paused ? "Resume" : "Pause");
    lv_obj_set_style_bg_color(g_pause_btn, lv_color_hex(is_paused ? 0x2ecc71 : 0x3465a4), LV_PART_MAIN);
    // Pause/resume/stop only make sense while a job is active.
    if (is_running || is_paused) {
        lv_obj_clear_state(g_pause_btn, LV_STATE_DISABLED);
        lv_obj_clear_state(g_stop_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(g_pause_btn, LV_STATE_DISABLED);
        lv_obj_add_state(g_stop_btn, LV_STATE_DISABLED);
    }

    if (g_light_btn_label) lv_label_set_text(g_light_btn_label, s.light_on ? "Light: On" : "Light: Off");
    lv_obj_set_style_bg_color(g_light_btn, lv_color_hex(s.light_on ? 0xf1c40f : 0x555555), LV_PART_MAIN);

    if (g_fan_btn_label) lv_label_set_text_fmt(g_fan_btn_label, "Fan %d%%", s.fan_speed_pct);

    // Reflect the printer's current speed in the dropdown, but don't fight the
    // user while they have the list open.
    if (g_speed_dd && !lv_dropdown_is_open(g_speed_dd)) {
        int sel = s.speed_level - 1;
        if (sel < 0) sel = 0;
        if (sel > 3) sel = 3;
        if ((int)lv_dropdown_get_selected(g_speed_dd) != sel) lv_dropdown_set_selected(g_speed_dd, sel);
    }
}

void update_ota_progress(int pct, const char* msg) {
    if (!g_update_screen) return;

    if (msg && g_update_label) {
        lv_label_set_text(g_update_label, msg);
    }

    if (g_update_bar && pct >= 0) {
        lv_bar_set_value(g_update_bar, pct, LV_ANIM_ON);
        if (g_update_pct_label) lv_label_set_text_fmt(g_update_pct_label, "%d%%", pct);
    }

    lv_timer_handler();
}

void show_update_screen() {
    if (g_update_screen) {
        lv_scr_load(g_update_screen);
        return;
    }

    g_update_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_update_screen, lv_color_hex(0x1a1a1a), LV_PART_MAIN);

    lv_obj_t* cont = lv_obj_create(g_update_screen);
    lv_obj_set_size(cont, 400, 220);
    lv_obj_center(cont);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xffaa00), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(cont, 10, LV_PART_MAIN);

    g_update_label = lv_label_create(cont);
    lv_label_set_text(g_update_label, "Updating System...");
    lv_obj_set_style_text_color(g_update_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_align(g_update_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_update_label, LV_ALIGN_TOP_MID, 0, 20);

    g_update_bar = lv_bar_create(cont);
    lv_obj_set_size(g_update_bar, 300, 20);
    lv_obj_align(g_update_bar, LV_ALIGN_CENTER, 0, 10);
    lv_bar_set_range(g_update_bar, 0, 100);
    lv_bar_set_value(g_update_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_update_bar, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_update_bar, lv_color_hex(0xffaa00), LV_PART_INDICATOR);

    g_update_pct_label = lv_label_create(cont);
    lv_label_set_text(g_update_pct_label, "0%");
    lv_obj_set_style_text_font(g_update_pct_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_update_pct_label, LV_ALIGN_CENTER, 0, 35);

    lv_obj_t* spinner = lv_spinner_create(cont);
    lv_obj_set_size(spinner, 30, 30);
    lv_obj_align(spinner, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0xffaa00), LV_PART_INDICATOR);

    lv_scr_load(g_update_screen);
    lv_timer_handler();
}
