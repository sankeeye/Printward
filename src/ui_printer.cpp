#include "ui_printer.h"
#include "storage.h"
#include "bambu_mqtt.h"
#include "ui_settings.h"
#include "ui_filament.h"
#include "ui_files.h"
#include "pt/pt_display.h"

lv_obj_t* g_main_screen = nullptr;
volatile int g_pending_action = ACT_NONE;
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
static lv_obj_t* g_speed_btn = nullptr;
static lv_obj_t* g_speed_btn_label = nullptr;
static lv_obj_t* g_slider = nullptr;

static void act_cb(lv_event_t* e) {
    int action = (int)(uintptr_t)lv_event_get_user_data(e);
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

static void filament_btn_cb(lv_event_t* e) {
    create_filament_ui();
}

static void files_btn_cb(lv_event_t* e) {
    create_files_ui();
}

static lv_obj_t* make_row(lv_obj_t* parent, int32_t height) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), height);
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
    lv_obj_set_height(btn, 64);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)action);
    if (label_out) *label_out = label;
    return btn;
}

void create_printer_ui() {
    lv_obj_clean(g_main_screen);
    lv_obj_set_style_bg_color(g_main_screen, lv_color_hex(g_bg_color), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(g_main_screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_center(root);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Header: wifi/ip (left) + printer connection (right) ---
    lv_obj_t* header = make_row(root, 30);
    g_wifi_label = lv_label_create(header);
    lv_obj_set_style_text_font(g_wifi_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_wifi_label, lv_color_hex(0xAAAAAA), 0);

    g_conn_label = lv_label_create(header);
    lv_obj_set_style_text_font(g_conn_label, &lv_font_montserrat_14, 0);

    lv_obj_t* filament_btn = lv_btn_create(header);
    lv_obj_set_size(filament_btn, 90, 26);
    lv_obj_set_style_bg_color(filament_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* filament_label = lv_label_create(filament_btn);
    lv_label_set_text(filament_label, "Filament");
    lv_obj_set_style_text_font(filament_label, &lv_font_montserrat_12, 0);
    lv_obj_center(filament_label);
    lv_obj_add_event_cb(filament_btn, filament_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* files_btn = lv_btn_create(header);
    lv_obj_set_size(files_btn, 80, 26);
    lv_obj_set_style_bg_color(files_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* files_label = lv_label_create(files_btn);
    lv_label_set_text(files_label, "Files");
    lv_obj_set_style_text_font(files_label, &lv_font_montserrat_12, 0);
    lv_obj_center(files_label);
    lv_obj_add_event_cb(files_btn, files_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* settings_btn = lv_btn_create(header);
    lv_obj_set_size(settings_btn, 90, 26);
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
    lv_obj_set_height(g_bar, 18);
    lv_bar_set_range(g_bar, 0, 100);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x2ecc71), LV_PART_INDICATOR);

    g_pct_label = lv_label_create(prog_row);
    lv_label_set_text(g_pct_label, "0%");
    lv_obj_set_style_text_font(g_pct_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_left(g_pct_label, 10, 0);

    g_remain_label = lv_label_create(prog_row);
    lv_label_set_text(g_remain_label, "");
    lv_obj_set_style_text_font(g_remain_label, &lv_font_montserrat_14, 0);
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
    lv_obj_set_size(g_ams_row, lv_pct(100), 80);
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
        lv_obj_set_size(box, 280, 80);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x1c1c1c), LV_PART_MAIN);
        lv_obj_set_style_border_color(box, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_pad_all(box, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_gap(box, 4, LV_PART_MAIN);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        g_ams_boxes[u] = box;
        for (int t = 0; t < AMS_MAX_TRAYS; t++) {
            lv_obj_t* cell = lv_obj_create(box);
            lv_obj_set_size(cell, 62, 68);
            lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(cell, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_gap(cell, 3, LV_PART_MAIN);
            lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* dot = lv_obj_create(cell);
            lv_obj_set_size(dot, 50, 34);
            lv_obj_set_style_radius(dot, 6, LV_PART_MAIN);
            lv_obj_set_style_border_width(dot, 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(dot, lv_color_hex(0x555555), LV_PART_MAIN);
            lv_obj_set_style_bg_color(dot, lv_color_hex(0x333333), LV_PART_MAIN);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
            g_ams_tray_dots[u][t] = dot;

            lv_obj_t* type_label = lv_label_create(cell);
            lv_label_set_text(type_label, "-");
            lv_obj_set_style_text_font(type_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(type_label, lv_color_hex(0xAAAAAA), 0);
            g_ams_tray_labels[u][t] = type_label;
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
    lv_obj_set_size(g_ext_swatch, 40, 28);
    lv_obj_set_style_radius(g_ext_swatch, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_ext_swatch, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_ext_swatch, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ext_swatch, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_clear_flag(g_ext_swatch, LV_OBJ_FLAG_SCROLLABLE);

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
    g_speed_btn = make_btn(ctrl_row, "Speed", 0x555555, ACT_SPEED_CYCLE, &g_speed_btn_label);

    // --- Brightness ---
    lv_obj_t* bright_row = make_row(root, 36);
    lv_obj_t* bl = lv_label_create(bright_row);
    lv_label_set_text(bl, "Brightness");
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
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

    if (g_speed_btn_label) lv_label_set_text(g_speed_btn_label, speed_name(s.speed_level));
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
