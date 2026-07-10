// Android-only idle screensaver / print dashboard. Sits on LVGL's top layer
// (below the brightness dim overlay so it dims too, above the normal screens),
// hidden until there has been no touch for SS_TIMEOUT_MS. Any tap hides it.
#include "ui_screensaver.h"
#include "bambu_mqtt.h"
#include <lvgl.h>
#include <ctime>
#include <cstring>

#define SS_TIMEOUT_MS 60000   // show after 60 s of no touch

static lv_obj_t* g_ss = nullptr;
static lv_obj_t* g_ss_clock = nullptr;
static lv_obj_t* g_ss_date = nullptr;
static lv_obj_t* g_ss_name = nullptr;
static lv_obj_t* g_ss_pct = nullptr;
static lv_obj_t* g_ss_bar = nullptr;
static lv_obj_t* g_ss_layers = nullptr;
static lv_obj_t* g_ss_time = nullptr;
static lv_obj_t* g_ss_ams = nullptr;
static lv_obj_t* g_ss_temps = nullptr;
static bool g_ss_shown = false;

static lv_obj_t* ss_label(lv_obj_t* parent, const lv_font_t* font, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, "");
    return l;
}

static void ss_click_cb(lv_event_t* e) {
    (void)e;
    lv_obj_add_flag(g_ss, LV_OBJ_FLAG_HIDDEN);
    g_ss_shown = false;
}

void create_screensaver() {
    g_ss = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_ss);
    lv_obj_set_size(g_ss, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(g_ss, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_ss, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(g_ss, 24, 0);
    lv_obj_set_style_pad_gap(g_ss, 8, 0);
    lv_obj_set_flex_flow(g_ss, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_ss, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(g_ss, LV_OBJ_FLAG_CLICKABLE);   // catch the wake tap
    lv_obj_clear_flag(g_ss, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_ss, ss_click_cb, LV_EVENT_CLICKED, NULL);

    g_ss_clock  = ss_label(g_ss, &lv_font_montserrat_48, 0xFFFFFF);
    g_ss_date   = ss_label(g_ss, &lv_font_montserrat_22, 0x888888);
    g_ss_name   = ss_label(g_ss, &lv_font_montserrat_28, 0x2ecc71);
    lv_label_set_long_mode(g_ss_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_ss_name, lv_pct(90));
    lv_obj_set_style_text_align(g_ss_name, LV_TEXT_ALIGN_CENTER, 0);

    g_ss_pct    = ss_label(g_ss, &lv_font_montserrat_48, 0xFFFFFF);

    g_ss_bar = lv_bar_create(g_ss);
    lv_obj_set_size(g_ss_bar, lv_pct(80), 22);
    lv_bar_set_range(g_ss_bar, 0, 100);
    lv_obj_set_style_bg_color(g_ss_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ss_bar, lv_color_hex(0x2ecc71), LV_PART_INDICATOR);

    g_ss_layers = ss_label(g_ss, &lv_font_montserrat_28, 0xFFFFFF);
    g_ss_time   = ss_label(g_ss, &lv_font_montserrat_28, 0xFFFFFF);
    g_ss_ams    = ss_label(g_ss, &lv_font_montserrat_22, 0xAAAAAA);
    g_ss_temps  = ss_label(g_ss, &lv_font_montserrat_22, 0xAAAAAA);

    lv_obj_add_flag(g_ss, LV_OBJ_FLAG_HIDDEN);   // hidden until idle
}

static void ss_update() {
    PrinterStatus& s = g_printer_status;

    // Wall-clock time + date (device local time).
    time_t now = time(nullptr);
    struct tm* lt = localtime(&now);
    char buf[64];
    if (lt) {
        strftime(buf, sizeof(buf), "%H:%M", lt);
        lv_label_set_text(g_ss_clock, buf);
        strftime(buf, sizeof(buf), "%A %d %B", lt);
        lv_label_set_text(g_ss_date, buf);
    }

    bool active = (strcmp(s.gcode_state, "RUNNING") == 0 || strcmp(s.gcode_state, "PAUSE") == 0);
    if (active) {
        lv_label_set_text(g_ss_name, s.task_name[0] ? s.task_name : "Printing");
        lv_label_set_text_fmt(g_ss_pct, "%d%%", s.progress_pct);
        lv_bar_set_value(g_ss_bar, s.progress_pct, LV_ANIM_OFF);
        lv_obj_clear_flag(g_ss_bar, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(g_ss_layers, "Laag %d / %d", s.layer_num, s.total_layers);
        int h = s.remaining_min / 60, m = s.remaining_min % 60;
        lv_label_set_text_fmt(g_ss_time, "Nog %du %02dm", h, m);

        int tn = s.active_tray_now;
        if (tn < 0)            lv_label_set_text(g_ss_ams, "AMS-slot: -");
        else if (tn == 254)    lv_label_set_text(g_ss_ams, "Externe spoel");
        else                   lv_label_set_text_fmt(g_ss_ams, "AMS %d - slot %d",
                                                     tn / AMS_MAX_TRAYS + 1, tn % AMS_MAX_TRAYS + 1);
    } else {
        lv_label_set_text(g_ss_name, s.mqtt_connected ? "Printer klaar / idle" : "Geen verbinding");
        lv_label_set_text(g_ss_pct, "");
        lv_obj_add_flag(g_ss_bar, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_ss_layers, "");
        lv_label_set_text(g_ss_time, "");
        lv_label_set_text(g_ss_ams, "");
    }

    lv_label_set_text_fmt(g_ss_temps, "Nozzle %.0f\xC2\xB0   Bed %.0f\xC2\xB0   Chamber %.0f\xC2\xB0",
                          s.nozzle_temp, s.bed_temp, s.chamber_temp);
}

void screensaver_loop() {
    if (!g_ss) return;

    uint32_t inactive = lv_display_get_inactive_time(NULL);
    if (inactive > SS_TIMEOUT_MS) {
        if (!g_ss_shown) {
            g_ss_shown = true;
            ss_update();
            lv_obj_clear_flag(g_ss, LV_OBJ_FLAG_HIDDEN);
        }
        static uint32_t last = 0;
        uint32_t t = lv_tick_get();
        if (t - last > 1000) { last = t; ss_update(); }   // refresh ~1x/sec
    } else if (g_ss_shown) {
        g_ss_shown = false;
        lv_obj_add_flag(g_ss, LV_OBJ_FLAG_HIDDEN);
    }
}
