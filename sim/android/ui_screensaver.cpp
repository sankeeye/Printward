// Android-only idle screensaver. Sits on LVGL's top layer (below the brightness
// dim overlay, above the normal screens), hidden until there's been no touch for
// SS_TIMEOUT_MS. It alternates two pages every SS_PAGE_MS: (1) a print dashboard
// and (2) a live top-down G-code build-up drawn up to the current layer. Any tap
// hides it.
#include "ui_screensaver.h"
#include "bambu_mqtt.h"
#include "gcode_view.h"
#include <lvgl.h>
#include <ctime>
#include <cstring>
#include <cstdlib>

#define SS_TIMEOUT_MS 60000   // show after 60 s of no touch
#define SS_PAGE_MS    10000   // switch page every 10 s

#define CV_W 760              // g-code canvas size (px)
#define CV_H 600

static const char* NL_DAYS[7] = {
    "Zondag", "Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag"
};
static const char* NL_MONTHS[12] = {
    "januari", "februari", "maart", "april", "mei", "juni",
    "juli", "augustus", "september", "oktober", "november", "december"
};

static lv_obj_t* g_ss = nullptr;
static lv_obj_t* g_page1 = nullptr;   // dashboard
static lv_obj_t* g_page2 = nullptr;   // g-code view
// dashboard widgets
static lv_obj_t* g_clock, *g_date, *g_name, *g_pct, *g_bar, *g_layers, *g_time, *g_ams, *g_temps;
// g-code page
static lv_obj_t* g_gcanvas = nullptr;
static lv_obj_t* g_ghdr = nullptr;
static uint16_t* g_cv = nullptr;       // RGB565 canvas buffer
static int g_rendered_layer = -99;

static bool g_shown = false;
static int  g_page = 0;

bool g_screensaver_3d = false;   // false = top-down 2D, true = isometric 3D

static lv_obj_t* mk_label(lv_obj_t* parent, const lv_font_t* font, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, "");
    return l;
}

static void ss_click_cb(lv_event_t*) {
    lv_obj_add_flag(g_ss, LV_OBJ_FLAG_HIDDEN);
    g_shown = false;
}

// ---- g-code rendering (direct RGB565 buffer writes) -------------------------
static inline void cv_plot(int x, int y, uint16_t c) {
    if ((unsigned)x < CV_W && (unsigned)y < CV_H) g_cv[y * CV_W + x] = c;
}
static void cv_line(int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        cv_plot(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void render_gcode(int cur_layer) {
    g_rendered_layer = cur_layer;
    if (!g_cv) return;
    memset(g_cv, 0, (size_t)CV_W * CV_H * 2);
    if (!gcode_view_ready()) { if (g_gcanvas) lv_obj_invalidate(g_gcanvas); return; }

    int16_t minx, miny, maxx, maxy;
    gcode_view_bounds(&minx, &miny, &maxx, &maxy);
    const GcodeSeg* segs = gcode_view_segments();
    int n = gcode_view_seg_count();
    const uint16_t grey = 0x52AA, accent = 0x07E0;   // dim grey, green

    if (!g_screensaver_3d) {
        // --- top-down 2D ---
        float w = (float)(maxx - minx), h = (float)(maxy - miny);
        if (w <= 0 || h <= 0) { if (g_gcanvas) lv_obj_invalidate(g_gcanvas); return; }
        float margin = 24;
        float s = (CV_W - 2 * margin) / w; float sy = (CV_H - 2 * margin) / h; if (sy < s) s = sy;
        float ox = (CV_W - w * s) * 0.5f, oy = (CV_H - h * s) * 0.5f;
        for (int i = 0; i < n; i++) {
            const GcodeSeg& g = segs[i];
            if (g.layer > cur_layer) continue;
            uint16_t col = ((int)g.layer >= cur_layer) ? accent : grey;
            int x1 = (int)(ox + (g.x1 - minx) * s);
            int y1 = (int)(CV_H - (oy + (g.y1 - miny) * s));   // flip Y (bed up)
            int x2 = (int)(ox + (g.x2 - minx) * s);
            int y2 = (int)(CV_H - (oy + (g.y2 - miny) * s));
            cv_line(x1, y1, x2, y2, col);
        }
    } else {
        // --- isometric 3D (z up) ---
        int16_t maxz = gcode_view_max_z();
        float cx = (minx + maxx) * 0.5f, cy = (miny + maxy) * 0.5f;
        const float C = 0.8660254f, S = 0.5f;      // cos/sin 30 deg
        // iso bounds from the 8 bounding-box corners
        float ixmin = 1e9f, ixmax = -1e9f, iymin = 1e9f, iymax = -1e9f;
        for (int k = 0; k < 8; k++) {
            float X = (k & 1) ? maxx : minx, Y = (k & 2) ? maxy : miny, Z = (k & 4) ? (float)maxz : 0;
            float dx = X - cx, dy = Y - cy;
            float ix = (dx - dy) * C, iy = (dx + dy) * S - Z;
            if (ix < ixmin) ixmin = ix; if (ix > ixmax) ixmax = ix;
            if (iy < iymin) iymin = iy; if (iy > iymax) iymax = iy;
        }
        float w = ixmax - ixmin, h = iymax - iymin;
        if (w <= 0 || h <= 0) { if (g_gcanvas) lv_obj_invalidate(g_gcanvas); return; }
        float margin = 30;
        float s = (CV_W - 2 * margin) / w; float sy = (CV_H - 2 * margin) / h; if (sy < s) s = sy;
        float ox = (CV_W - w * s) * 0.5f, oy = (CV_H - h * s) * 0.5f;
        for (int i = 0; i < n; i++) {
            const GcodeSeg& g = segs[i];
            if (g.layer > cur_layer) continue;
            uint16_t col = ((int)g.layer >= cur_layer) ? accent : grey;
            float dx1 = g.x1 - cx, dy1 = g.y1 - cy;
            float dx2 = g.x2 - cx, dy2 = g.y2 - cy;
            int X1 = (int)(ox + ((dx1 - dy1) * C - ixmin) * s);
            int Y1 = (int)(CV_H - (oy + (((dx1 + dy1) * S - g.z) - iymin) * s));
            int X2 = (int)(ox + ((dx2 - dy2) * C - ixmin) * s);
            int Y2 = (int)(CV_H - (oy + (((dx2 + dy2) * S - g.z) - iymin) * s));
            cv_line(X1, Y1, X2, Y2, col);
        }
    }
    if (g_gcanvas) lv_obj_invalidate(g_gcanvas);
}

void screensaver_view_changed() {
    g_rendered_layer = -99;   // force a redraw on the next gcode-page refresh
}

void create_screensaver() {
    g_ss = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_ss);
    lv_obj_set_size(g_ss, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(g_ss, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_ss, LV_OPA_COVER, 0);
    lv_obj_add_flag(g_ss, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_ss, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_ss, ss_click_cb, LV_EVENT_CLICKED, NULL);

    // --- Page 1: dashboard ---
    g_page1 = lv_obj_create(g_ss);
    lv_obj_remove_style_all(g_page1);
    lv_obj_set_size(g_page1, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(g_page1, 24, 0);
    lv_obj_set_style_pad_gap(g_page1, 8, 0);
    lv_obj_set_flex_flow(g_page1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_page1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_page1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_page1, LV_OBJ_FLAG_SCROLLABLE);

    g_clock = mk_label(g_page1, &lv_font_montserrat_48, 0xFFFFFF);
    g_date  = mk_label(g_page1, &lv_font_montserrat_22, 0x888888);
    g_name  = mk_label(g_page1, &lv_font_montserrat_28, 0x2ecc71);
    lv_label_set_long_mode(g_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_name, lv_pct(90));
    lv_obj_set_style_text_align(g_name, LV_TEXT_ALIGN_CENTER, 0);
    g_pct = mk_label(g_page1, &lv_font_montserrat_48, 0xFFFFFF);
    g_bar = lv_bar_create(g_page1);
    lv_obj_set_size(g_bar, lv_pct(80), 22);
    lv_bar_set_range(g_bar, 0, 100);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x2ecc71), LV_PART_INDICATOR);
    g_layers = mk_label(g_page1, &lv_font_montserrat_28, 0xFFFFFF);
    g_time   = mk_label(g_page1, &lv_font_montserrat_28, 0xFFFFFF);
    g_ams    = mk_label(g_page1, &lv_font_montserrat_22, 0xAAAAAA);
    g_temps  = mk_label(g_page1, &lv_font_montserrat_22, 0xAAAAAA);

    // --- Page 2: g-code build-up ---
    g_page2 = lv_obj_create(g_ss);
    lv_obj_remove_style_all(g_page2);
    lv_obj_set_size(g_page2, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(g_page2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_page2, LV_OBJ_FLAG_SCROLLABLE);

    g_ghdr = mk_label(g_page2, &lv_font_montserrat_22, 0xFFFFFF);
    lv_obj_align(g_ghdr, LV_ALIGN_TOP_MID, 0, 12);

    g_cv = (uint16_t*)malloc((size_t)CV_W * CV_H * 2);
    if (g_cv) {
        memset(g_cv, 0, (size_t)CV_W * CV_H * 2);
        g_gcanvas = lv_canvas_create(g_page2);
        lv_canvas_set_buffer(g_gcanvas, g_cv, CV_W, CV_H, LV_COLOR_FORMAT_RGB565);
        lv_obj_align(g_gcanvas, LV_ALIGN_CENTER, 0, 20);
    }

    lv_obj_add_flag(g_page2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ss, LV_OBJ_FLAG_HIDDEN);
}

static void update_dashboard() {
    PrinterStatus& s = g_printer_status;
    time_t now = time(nullptr);
    struct tm* lt = localtime(&now);
    char buf[64];
    if (lt) {
        strftime(buf, sizeof(buf), "%H:%M", lt);
        lv_label_set_text(g_clock, buf);
        int wd = (lt->tm_wday >= 0 && lt->tm_wday < 7) ? lt->tm_wday : 0;
        int mo = (lt->tm_mon >= 0 && lt->tm_mon < 12) ? lt->tm_mon : 0;
        lv_label_set_text_fmt(g_date, "%s %d %s", NL_DAYS[wd], lt->tm_mday, NL_MONTHS[mo]);
    }
    bool active = (strcmp(s.gcode_state, "RUNNING") == 0 || strcmp(s.gcode_state, "PAUSE") == 0);
    if (active) {
        lv_label_set_text(g_name, s.task_name[0] ? s.task_name : "Printing");
        lv_label_set_text_fmt(g_pct, "%d%%", s.progress_pct);
        lv_bar_set_value(g_bar, s.progress_pct, LV_ANIM_OFF);
        lv_obj_clear_flag(g_bar, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(g_layers, "Laag %d / %d", s.layer_num, s.total_layers);
        lv_label_set_text_fmt(g_time, "Nog %du %02dm", s.remaining_min / 60, s.remaining_min % 60);
        int tn = s.active_tray_now;
        if (tn < 0)         lv_label_set_text(g_ams, "AMS-slot: -");
        else if (tn == 254) lv_label_set_text(g_ams, "Externe spoel");
        else                lv_label_set_text_fmt(g_ams, "AMS %d - slot %d",
                                                  tn / AMS_MAX_TRAYS + 1, tn % AMS_MAX_TRAYS + 1);
    } else {
        lv_label_set_text(g_name, s.mqtt_connected ? "Printer klaar / idle" : "Geen verbinding");
        lv_label_set_text(g_pct, "");
        lv_obj_add_flag(g_bar, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_layers, "");
        lv_label_set_text(g_time, "");
        lv_label_set_text(g_ams, "");
    }
    lv_label_set_text_fmt(g_temps, "Nozzle %.0f\xC2\xB0   Bed %.0f\xC2\xB0   Chamber %.0f\xC2\xB0",
                          s.nozzle_temp, s.bed_temp, s.chamber_temp);
}

static void update_gcode_page() {
    PrinterStatus& s = g_printer_status;
    if (gcode_view_ready()) {
        lv_label_set_text_fmt(g_ghdr, "%s    laag %d/%d    %d%%",
                              s.task_name, s.layer_num, s.total_layers, s.progress_pct);
        int ml = gcode_view_max_layer();
        bool active = (strcmp(s.gcode_state, "RUNNING") == 0 || strcmp(s.gcode_state, "PAUSE") == 0);
        int cur = active ? s.layer_num : ml;   // finished/idle -> show the whole model
        if (cur > ml) cur = ml;
        if (cur < 0) cur = 0;
        if (cur != g_rendered_layer) render_gcode(cur);
    } else {
        lv_label_set_text(g_ghdr, "Model laden...");
        if (g_rendered_layer != -1) render_gcode(-1);   // clear once
    }
}

void screensaver_loop() {
    if (!g_ss) return;
    static uint32_t page_at = 0, refresh_at = 0;

    uint32_t inactive = lv_display_get_inactive_time(NULL);
    if (inactive > SS_TIMEOUT_MS) {
        uint32_t t = lv_tick_get();
        if (!g_shown) {
            g_shown = true;
            g_page = 0;
            lv_obj_clear_flag(g_page1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_page2, LV_OBJ_FLAG_HIDDEN);
            update_dashboard();
            lv_obj_clear_flag(g_ss, LV_OBJ_FLAG_HIDDEN);
            page_at = refresh_at = t;
        }
        if (t - page_at > SS_PAGE_MS) {
            page_at = t;
            g_page ^= 1;
            if (g_page == 0) {
                lv_obj_clear_flag(g_page1, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(g_page2, LV_OBJ_FLAG_HIDDEN);
                update_dashboard();
            } else {
                lv_obj_add_flag(g_page1, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(g_page2, LV_OBJ_FLAG_HIDDEN);
                g_rendered_layer = -99;   // force a redraw
                update_gcode_page();
            }
        }
        if (t - refresh_at > 1000) {
            refresh_at = t;
            if (g_page == 0) update_dashboard();
            else update_gcode_page();
        }
    } else if (g_shown) {
        g_shown = false;
        lv_obj_add_flag(g_ss, LV_OBJ_FLAG_HIDDEN);
    }
}
