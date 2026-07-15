// "Scale" screen (tablet only): live weight + tare/calibrate + network
// management for the FilaTrack Scale. Everything the scale's own web page can do,
// on the tablet. All scale I/O goes through scale_client (a background thread);
// this file only builds the LVGL UI and reads/queues via that client.
#include "ui_weigh.h"
#include "lang.h"
#include "ui_printer.h"
#include "ui_settings.h"   // return to Settings on Back (the scale opens from there)
#include "scale_client.h"
#include "filament_track.h"   // filament_weigh_assign()
#include "spool_db.h"         // g_empties (empty-spool library)
#include "storage.h"       // g_scale_ip, save_settings()
#include <lvgl.h>
#include <cstdio>
#include <cstring>
#include "ui_scale.h"      // PT_SZ + font remap (Android)

static lv_obj_t* g_screen = nullptr;
static lv_obj_t* g_weight = nullptr;
static lv_obj_t* g_sub = nullptr;      // stable/online line
static lv_obj_t* g_infolbl = nullptr;
static lv_obj_t* g_msg = nullptr;
static lv_obj_t* g_ta_known = nullptr;
static lv_obj_t* g_ta_ssid = nullptr;
static lv_obj_t* g_ta_pass = nullptr;
static lv_obj_t* g_ta_ip = nullptr;
static lv_obj_t* g_kb = nullptr;

// percent-encode a query value into out (bounded).
static void urlenc(const char* in, char* out, int n) {
    static const char* hex = "0123456789ABCDEF";
    int j = 0;
    for (const unsigned char* s = (const unsigned char*)in; *s && j < n - 4; s++) {
        unsigned char c = *s;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out[j++] = (char)c;
        } else {
            out[j++] = '%'; out[j++] = hex[c >> 4]; out[j++] = hex[c & 0xF];
        }
    }
    out[j] = 0;
}

static void kb_hide() {
    if (!g_kb) return;
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(g_kb, nullptr);
}

static void ta_focus_cb(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    bool num = (ta == g_ta_known);
    lv_keyboard_set_mode(g_kb, num ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(g_kb, ta);
    lv_obj_clear_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_kb);
}

static void kb_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) kb_hide();
}

static void tare_cb(lv_event_t*) { scale_cmd("/tare"); }

static void cal_cb(lv_event_t*) {
    const char* k = lv_textarea_get_text(g_ta_known);
    if (!k || !k[0]) return;
    char path[64];
    snprintf(path, sizeof(path), "/cal?known=%s", k);
    scale_cmd(path);
}

static void wifi_cb(lv_event_t*) {
    const char* ssid = lv_textarea_get_text(g_ta_ssid);
    const char* pass = lv_textarea_get_text(g_ta_pass);
    if (!ssid || !ssid[0]) { if (g_msg) lv_label_set_text(g_msg, T("scale.need_ssid")); return; }
    char es[96], ep[96], path[256];
    urlenc(ssid, es, sizeof(es));
    urlenc(pass ? pass : "", ep, sizeof(ep));
    snprintf(path, sizeof(path), "/setwifi?ssid=%s&pass=%s", es, ep);
    scale_cmd(path);
    kb_hide();
}

// Save the scale host the tablet talks to (persist in the conf).
static void ip_save_cb(lv_event_t*) {
    const char* ip = lv_textarea_get_text(g_ta_ip);
    if (!ip || !ip[0]) return;
    strncpy(g_scale_ip, ip, sizeof(g_scale_ip) - 1);
    g_scale_ip[sizeof(g_scale_ip) - 1] = 0;
    save_settings();
    if (g_msg) lv_label_set_text(g_msg, T("scale.ip_saved"));
    kb_hide();
}

// Pin that IP as the scale's own fixed IP (over HTTP to the scale).
static void ip_fix_cb(lv_event_t*) {
    const char* ip = lv_textarea_get_text(g_ta_ip);
    if (!ip || !ip[0]) return;
    char path[64];
    snprintf(path, sizeof(path), "/setip?ip=%s", ip);
    scale_cmd(path);
    kb_hide();
}

static void back_cb(lv_event_t*) {
    scale_set_polling(false);
    create_settings_ui();   // opened from Settings, so return there
}

static lv_obj_t* make_btn(lv_obj_t* parent, const char* txt, uint32_t color, lv_event_cb_t cb, int w) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_size(b, PT_SZ(w), PT_SZ(46));
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

static lv_obj_t* make_ta(lv_obj_t* parent, const char* ph, const char* val, bool pw, int w) {
    lv_obj_t* ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, PT_SZ(w), PT_SZ(46));
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, pw);
    lv_textarea_set_placeholder_text(ta, ph);
    if (val && val[0]) lv_textarea_set_text(ta, val);
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_CLICKED, nullptr);
    return ta;
}

static lv_obj_t* make_row(lv_obj_t* parent) {
    lv_obj_t* r = lv_obj_create(parent);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(r, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(r, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(r, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    return r;
}

void create_weigh_ui() {
    if (g_screen) { lv_obj_del(g_screen); g_screen = nullptr; }
    g_kb = nullptr;

    g_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_screen, lv_color_hex(0x101418), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(g_screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t* header = make_row(root);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, T("nav.scale"));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_t* back = lv_btn_create(header);
    lv_obj_set_size(back, PT_SZ(80), PT_SZ(34));
    lv_obj_set_style_bg_color(back, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* bl = lv_label_create(back); lv_label_set_text(bl, T("back"));
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0); lv_obj_center(bl);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, nullptr);

    // Big weight
    g_weight = lv_label_create(root);
    lv_label_set_text(g_weight, "- g");
    lv_obj_set_style_text_font(g_weight, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(g_weight, lv_color_hex(0xFFFFFF), 0);
    g_sub = lv_label_create(root);
    lv_label_set_text(g_sub, T("scale.connecting"));
    lv_obj_set_style_text_font(g_sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_sub, lv_color_hex(0x999999), 0);

    // Tare + calibrate
    lv_obj_t* r1 = make_row(root);
    make_btn(r1, "Tarra", 0x2c3e50, tare_cb, 120);
    g_ta_known = make_ta(r1, "gram", "500", false, 110);
    make_btn(r1, "Kalibreer", 0x27ae60, cal_cb, 130);

    // (Weigh-to-slot moved to the Spools tab: weigh a named roll there and
    // assign it with "Rol" - that links the roll, price and printer AMS. This
    // screen is just for managing the scale itself.)

    // Network info
    g_infolbl = lv_label_create(root);
    lv_label_set_text(g_infolbl, T("scale.net_wait"));
    lv_obj_set_style_text_font(g_infolbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_infolbl, lv_color_hex(0x93a0ad), 0);

    // Scale IP row
    lv_obj_t* r2 = make_row(root);
    g_ta_ip = make_ta(r2, "Scale IP", g_scale_ip, false, 200);
    make_btn(r2, "Opslaan", 0x3465a4, ip_save_cb, 110);
    make_btn(r2, "Vast op schaal", 0x555555, ip_fix_cb, 150);

    // WiFi row
    lv_obj_t* r3 = make_row(root);
    g_ta_ssid = make_ta(r3, "WiFi naam", "", false, 200);
    g_ta_pass = make_ta(r3, "wachtwoord", "", true, 160);
    make_btn(r3, "WiFi wijzigen", 0xe67e22, wifi_cb, 150);

    g_msg = lv_label_create(root);
    lv_label_set_text(g_msg, "");
    lv_obj_set_style_text_font(g_msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_msg, lv_color_hex(0x2ecc71), 0);

    // Shared keyboard (hidden until a field is focused)
    g_kb = lv_keyboard_create(g_screen);
    lv_obj_set_size(g_kb, lv_pct(100), PT_SZ(170));
    lv_obj_align(g_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_kb, kb_event_cb, LV_EVENT_ALL, nullptr);

    scale_set_polling(true);
    lv_scr_load(g_screen);
    update_weigh_ui();
}

void update_weigh_ui() {
    if (!g_screen || !g_weight) return;
    bool on = scale_online();
    if (on) lv_label_set_text_fmt(g_weight, "%.0f g", scale_grams());
    else lv_label_set_text(g_weight, "- g");
    lv_label_set_text(g_sub, on ? (scale_stable() ? T("scale.stable") : T("scale.measuring")) : T("scale.no_conn"));
    lv_obj_set_style_text_color(g_sub, lv_color_hex(on ? 0x2ecc71 : 0xe74c3c), 0);

    char info[256];
    scale_info(info, sizeof(info));
    if (info[0]) {
        // pull ip + ssid + rssi out of the /info JSON for a compact line
        char ip[40] = "", ssid[48] = ""; int rssi = 0;
        const char* p;
        if ((p = strstr(info, "\"ip\":\""))) sscanf(p + 6, "%39[^\"]", ip);
        if ((p = strstr(info, "\"ssid\":\""))) sscanf(p + 8, "%47[^\"]", ssid);
        if ((p = strstr(info, "\"rssi\":"))) rssi = atoi(p + 7);
        bool st = strstr(info, "\"static\":true") != nullptr;
        lv_label_set_text_fmt(g_infolbl, T("scale.net_fmt"), ip, ssid, rssi, st ? T("scale.static_suffix") : "");
    }

    char msg[128];
    scale_last_msg(msg, sizeof(msg));
    if (msg[0]) lv_label_set_text(g_msg, msg);
}
