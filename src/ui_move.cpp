// Manual motion / control screen: jog X/Y/Z, home, extrude/retract, preheat.
// Same idea as the stock firmware's "Move" tab. Every action is turned into
// plain g-code and pushed to the printer with bambu_cmd_gcode(). Moves use a
// relative block (G91 / G1 / G90) so the chosen step size is a delta from the
// current position; homing uses G28; hotend preheat/cooldown use M104.
//
// The action logic (move_blocked / move_perform) is factored out so the web
// control page (sim/android/webctl.cpp) drives the exact same guarded path.
#include "ui_move.h"
#include "ui_printer.h"
#include "bambu_mqtt.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "ui_scale.h"   // tablet font scaling (Android only)

static lv_obj_t* g_move_screen = nullptr;
static lv_obj_t* g_hint  = nullptr;   // status / error line at the bottom
static lv_obj_t* g_noz_label = nullptr;

// Jog step in mm, picked from the 0.1 / 1 / 10 selector.
static const float STEPS[3] = {0.1f, 1.0f, 10.0f};
static float g_step = 1.0f;
static lv_obj_t* g_step_btns[3] = {nullptr, nullptr, nullptr};

// Manual extrusion amount + feed (per tap). Cold extrusion is refused below
// this nozzle temperature (matches Marlin's default cold-extrude guard).
#define EXTRUDE_MM   10.0f
#define EXTRUDE_FEED 300
#define MIN_EXTRUDE_TEMP 170.0f
#define PREHEAT_TEMP 220

static bool is_printing() {
    const char* st = g_printer_status.gcode_state;
    return strcmp(st, "RUNNING") == 0 || strcmp(st, "PAUSE") == 0 ||
           strcmp(st, "PREPARE") == 0 || strcmp(st, "SLICING") == 0;
}

static void set_hint(const char* msg, uint32_t color) {
    if (!g_hint) return;
    lv_label_set_text(g_hint, msg);
    lv_obj_set_style_text_color(g_hint, lv_color_hex(color), 0);
}

// One relative move on a single axis, e.g. send_axis('Z', -1.0, 600) ->
// "G91\nG1 Z-1.00 F600\nG90".
static void send_axis(char axis, float dist, int feed) {
    char buf[80];
    snprintf(buf, sizeof(buf), "G91\nG1 %c%.2f F%d\nG90", axis, dist, feed);
    bambu_cmd_gcode(buf);
}

bool move_blocked(int code, const char** reason) {
    if (code == MOVE_PREHEAT || code == MOVE_COOL) return false;   // always allowed
    if (is_printing()) {
        *reason = "Bezig met printen \xE2\x80\x93 bewegen uitgeschakeld";
        return true;
    }
    if ((code == MOVE_EEXT || code == MOVE_ERET) && g_printer_status.nozzle_temp < MIN_EXTRUDE_TEMP) {
        *reason = "Nozzle te koud (<170\xC2\xB0" "C) \xE2\x80\x93 eerst opwarmen";
        return true;
    }
    return false;
}

const char* move_perform(int code, float step) {
    const char* reason = "";
    if (move_blocked(code, &reason)) { set_hint(reason, 0xf39c12); return reason; }

    switch (code) {
        case MOVE_XP: send_axis('X',  step, 3000); set_hint("X+", 0xFFFFFF); return "X+";
        case MOVE_XM: send_axis('X', -step, 3000); set_hint("X-", 0xFFFFFF); return "X-";
        case MOVE_YP: send_axis('Y',  step, 3000); set_hint("Y+", 0xFFFFFF); return "Y+";
        case MOVE_YM: send_axis('Y', -step, 3000); set_hint("Y-", 0xFFFFFF); return "Y-";
        case MOVE_ZP: send_axis('Z',  step, 600);  set_hint("Z+", 0xFFFFFF); return "Z+";
        case MOVE_ZM: send_axis('Z', -step, 600);  set_hint("Z-", 0xFFFFFF); return "Z-";
        case MOVE_HOME:
            bambu_cmd_gcode("G28");
            set_hint("Homing...", 0x2ecc71);
            return "Homing";
        case MOVE_EEXT:
            send_axis('E', EXTRUDE_MM, EXTRUDE_FEED);
            set_hint("Extruderen...", 0x27ae60);
            return "Extrude";
        case MOVE_ERET:
            send_axis('E', -EXTRUDE_MM, EXTRUDE_FEED);
            set_hint("Terugtrekken...", 0x27ae60);
            return "Retract";
        case MOVE_PREHEAT: {
            char buf[24];
            snprintf(buf, sizeof(buf), "M104 S%d", PREHEAT_TEMP);
            bambu_cmd_gcode(buf);
            set_hint("Opwarmen naar 220\xC2\xB0" "C...", 0xe67e22);
            return "Preheat";
        }
        case MOVE_COOL:
            bambu_cmd_gcode("M104 S0");
            set_hint("Nozzle uit", 0x2980b9);
            return "Cooldown";
    }
    return "";
}

static void jog_cb(lv_event_t* e) {
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    move_perform(code, g_step);
}

static void step_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    g_step = STEPS[idx];
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_bg_color(g_step_btns[i], lv_color_hex(i == idx ? 0x3465a4 : 0x333333), 0);
    }
}

static void back_btn_cb(lv_event_t* e) {
    lv_scr_load(g_main_screen);
}

// A square-ish action button with a centered label, wired to jog_cb via `code`.
static lv_obj_t* make_jog_btn(lv_obj_t* parent, const char* text, int code, uint32_t color) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, jog_cb, LV_EVENT_CLICKED, (void*)(intptr_t)code);
    return b;
}

// A transparent flex-column container (a "panel" holding a title + buttons).
static lv_obj_t* make_col(lv_obj_t* parent, int32_t width) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_size(c, PT_SZ(width), lv_pct(100));
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(c, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static lv_obj_t* make_col_title(lv_obj_t* parent, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0x999999), 0);
    return l;
}

void create_move_ui() {
    if (g_move_screen) {
        lv_obj_del(g_move_screen);
        g_move_screen = nullptr;
    }
    g_step_btns[0] = g_step_btns[1] = g_step_btns[2] = nullptr;

    g_move_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_move_screen, lv_color_hex(0x101418), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(g_move_screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_center(root);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Header: title + step selector + Back ---
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_set_size(header, lv_pct(100), PT_SZ(44));
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(header, PT_SZ(8), LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Move");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t* step_lbl = lv_label_create(header);
    lv_label_set_text(step_lbl, "Stap:");
    lv_obj_set_style_text_font(step_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(step_lbl, lv_color_hex(0x999999), 0);
    lv_obj_set_style_pad_left(step_lbl, PT_SZ(16), 0);

    const char* step_txt[3] = {"0.1", "1", "10"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t* sb = lv_btn_create(header);
        lv_obj_set_size(sb, PT_SZ(60), PT_SZ(34));
        lv_obj_set_style_bg_color(sb, lv_color_hex(g_step == STEPS[i] ? 0x3465a4 : 0x333333), LV_PART_MAIN);
        lv_obj_t* sl = lv_label_create(sb);
        lv_label_set_text(sl, step_txt[i]);
        lv_obj_set_style_text_font(sl, &lv_font_montserrat_14, 0);
        lv_obj_center(sl);
        lv_obj_add_event_cb(sb, step_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        g_step_btns[i] = sb;
    }
    lv_obj_t* mm_lbl = lv_label_create(header);
    lv_label_set_text(mm_lbl, "mm");
    lv_obj_set_style_text_font(mm_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mm_lbl, lv_color_hex(0x999999), 0);

    lv_obj_t* back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, PT_SZ(80), PT_SZ(34));
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_FLOATING);        // pin to the right edge
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    // --- Main row: XY pad | Z | Extruder ---
    lv_obj_t* main = lv_obj_create(root);
    lv_obj_set_width(main, lv_pct(100));
    lv_obj_set_flex_grow(main, 1);
    lv_obj_set_style_bg_opa(main, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(main, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(main, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(main, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_flex_flow(main, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(main, LV_OBJ_FLAG_SCROLLABLE);

    // XY jog pad: a 3x3 grid with the four arrows around a central Home.
    lv_obj_t* pad = lv_obj_create(main);
    lv_obj_set_size(pad, PT_SZ(280), PT_SZ(280));
    lv_obj_set_style_bg_opa(pad, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(pad, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(pad, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(pad, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_style_pad_column(pad, PT_SZ(6), LV_PART_MAIN);
    lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);
    static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(pad, col_dsc, row_dsc);

    struct { const char* t; int code; int col; int row; uint32_t c; } cells[] = {
        {"Y+", MOVE_YP, 1, 0, 0x2c3e50},
        {"X-", MOVE_XM, 0, 1, 0x2c3e50},
        {LV_SYMBOL_HOME, MOVE_HOME, 1, 1, 0xc0392b},
        {"X+", MOVE_XP, 2, 1, 0x2c3e50},
        {"Y-", MOVE_YM, 1, 2, 0x2c3e50},
    };
    for (auto& cell : cells) {
        lv_obj_t* b = make_jog_btn(pad, cell.t, cell.code, cell.c);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, cell.col, 1, LV_GRID_ALIGN_STRETCH, cell.row, 1);
    }

    // Z column.
    lv_obj_t* zcol = make_col(main, 110);
    make_col_title(zcol, "Z");
    lv_obj_t* zp = make_jog_btn(zcol, "Z+", MOVE_ZP, 0x2c3e50);
    lv_obj_set_size(zp, lv_pct(100), PT_SZ(84));
    lv_obj_t* zm = make_jog_btn(zcol, "Z-", MOVE_ZM, 0x2c3e50);
    lv_obj_set_size(zm, lv_pct(100), PT_SZ(84));

    // Extruder column.
    lv_obj_t* ecol = make_col(main, 220);
    lv_obj_set_flex_grow(ecol, 1);
    make_col_title(ecol, "Extruder");
    g_noz_label = lv_label_create(ecol);
    lv_obj_set_style_text_font(g_noz_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_noz_label, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t* ext = make_jog_btn(ecol, "Extrude", MOVE_EEXT, 0x27ae60);
    lv_obj_set_size(ext, lv_pct(100), PT_SZ(62));
    lv_obj_t* ret = make_jog_btn(ecol, "Retract", MOVE_ERET, 0xd35400);
    lv_obj_set_size(ret, lv_pct(100), PT_SZ(62));

    lv_obj_t* heat_row = lv_obj_create(ecol);
    lv_obj_set_size(heat_row, lv_pct(100), PT_SZ(48));
    lv_obj_set_style_bg_opa(heat_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(heat_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(heat_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(heat_row, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(heat_row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(heat_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* ph = make_jog_btn(heat_row, "Preheat", MOVE_PREHEAT, 0xe67e22);
    lv_obj_set_flex_grow(ph, 1); lv_obj_set_height(ph, lv_pct(100));
    lv_obj_t* cl = make_jog_btn(heat_row, "Cooldown", MOVE_COOL, 0x2980b9);
    lv_obj_set_flex_grow(cl, 1); lv_obj_set_height(cl, lv_pct(100));

    // --- Hint / status line ---
    g_hint = lv_label_create(root);
    lv_label_set_text(g_hint, "");
    lv_obj_set_style_text_font(g_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_hint, lv_color_hex(0x999999), 0);

    lv_scr_load(g_move_screen);
    update_move_ui();
}

void update_move_ui() {
    if (!g_move_screen || !g_noz_label) return;
    lv_label_set_text_fmt(g_noz_label, "Nozzle %.0f/%.0f\xC2\xB0" "C",
                          g_printer_status.nozzle_temp, g_printer_status.nozzle_target);
}
