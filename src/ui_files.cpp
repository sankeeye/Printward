#include "ui_files.h"
#include "lang.h"
#include "ui_printer.h"
#include "bambu_ftp.h"
#include "bambu_mqtt.h"
#include "storage.h"   // g_lan_mode
#include <lvgl.h>
#include "ui_scale.h"   // tablet font scaling (Android only)

static lv_obj_t* g_files_screen = nullptr;
static lv_obj_t* g_path_label = nullptr;
static lv_obj_t* g_status_label = nullptr;
static lv_obj_t* g_up_btn = nullptr;
static lv_obj_t* g_page_label = nullptr;

#define PAGE_SIZE 6
static lv_obj_t* g_row_btns[PAGE_SIZE] = {nullptr};
static lv_obj_t* g_row_labels[PAGE_SIZE] = {nullptr};

static String g_current_path = "/";
static FtpEntry g_entries[FTP_MAX_ENTRIES];
static int g_entry_count = 0;
static int g_page = 0;

// Set from click callbacks (up/refresh/open-folder buttons), drained in
// ui_files_loop() (called from the main loop, after the current frame has
// already been flushed) - bambu_ftp_list() does a couple of full TLS
// handshakes plus a directory transfer, easily a second or more, and running
// that straight from an LVGL click callback stalled lv_task_handler()
// mid-redraw and tore the frame (same class of bug as the WiFi
// Connect-and-Save issue - see g_connect_requested in ui_wifi.cpp for the
// same pattern applied there).
static volatile bool g_list_requested = false;

// Same reasoning for starting a print (bambu_cmd_start_project_file/gcode_file
// -> an MQTT publish) - deferred from confirm_print_cb() to ui_files_loop().
static volatile bool g_print_requested = false;
static String g_pending_ftp_path;
static bool g_pending_use_ams = false;
static int g_pending_mapping[5];
static bool g_pending_is_gcode = false;

// --- Print-confirm modal ---
#define MAX_COLOR_OPTIONS (AMS_MAX_UNITS * AMS_MAX_TRAYS + 2) // Skip + trays + External
static int g_color_options[MAX_COLOR_OPTIONS];
static String g_color_option_labels[MAX_COLOR_OPTIONS];
static int g_color_option_count = 0;
static int g_color_option_idx[4] = {0, 0, 0, 0};

static lv_obj_t* g_confirm_modal = nullptr;
static lv_obj_t* g_confirm_file_label = nullptr;
static lv_obj_t* g_ams_toggle_btn = nullptr;
static lv_obj_t* g_ams_toggle_label = nullptr;
static lv_obj_t* g_color_rows[4] = {nullptr};
static lv_obj_t* g_color_value_labels[4] = {nullptr};
static bool g_ams_enabled = false;
static bool g_confirm_is_3mf = false;
static String g_confirm_ftp_path;

static void back_btn_cb(lv_event_t* e) {
    lv_scr_load(g_main_screen);
}

static String join_path(const String& base, const String& name) {
    if (base == "/" || base.length() == 0) return "/" + name;
    return base + "/" + name;
}

static String parent_path(const String& path) {
    if (path == "/" || path.length() == 0) return "/";
    String p = path;
    if (p.endsWith("/")) p = p.substring(0, p.length() - 1);
    int idx = p.lastIndexOf('/');
    if (idx <= 0) return "/";
    return p.substring(0, idx);
}

static bool has_ext(const String& name, const char* ext) {
    size_t elen = strlen(ext);
    if (name.length() < elen) return false;
    String tail = name.substring(name.length() - elen);
    tail.toLowerCase();
    return tail == ext;
}

static String format_size(uint32_t bytes) {
    if (bytes >= 1024UL * 1024UL) {
        return String(bytes / (1024.0f * 1024.0f), 1) + " MB";
    } else if (bytes >= 1024) {
        return String(bytes / 1024) + " KB";
    }
    return String(bytes) + " B";
}

static void build_color_options() {
    g_color_option_count = 0;
    g_color_options[g_color_option_count] = -1;
    g_color_option_labels[g_color_option_count] = "Skip";
    g_color_option_count++;

    PrinterStatus& s = g_printer_status;
    for (int u = 0; u < AMS_MAX_UNITS; u++) {
        if (u >= s.ams_count || !s.ams[u].present) continue;
        for (int t = 0; t < AMS_MAX_TRAYS; t++) {
            if (!s.ams[u].trays[t].present) continue;
            g_color_options[g_color_option_count] = u * AMS_MAX_TRAYS + t;
            g_color_option_labels[g_color_option_count] = "AMS" + String(u + 1) + " T" + String(t + 1);
            g_color_option_count++;
        }
    }
    if (s.external_spool.present) {
        g_color_options[g_color_option_count] = 254;
        g_color_option_labels[g_color_option_count] = "External";
        g_color_option_count++;
    }
}

static void update_color_row_label(int i) {
    lv_label_set_text(g_color_value_labels[i], g_color_option_labels[g_color_option_idx[i]].c_str());
}

static void color_cycle_cb(lv_event_t* e) {
    int i = (int)(uintptr_t)lv_event_get_user_data(e);
    g_color_option_idx[i] = (g_color_option_idx[i] + 1) % g_color_option_count;
    update_color_row_label(i);
}

static void update_ams_rows_visibility() {
    for (int i = 0; i < 4; i++) {
        if (g_ams_enabled) lv_obj_clear_flag(g_color_rows[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_color_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(g_ams_toggle_label, g_ams_enabled ? T("files.ams_on") : T("files.ams_off_ext"));
}

static void ams_toggle_cb(lv_event_t* e) {
    g_ams_enabled = !g_ams_enabled;
    update_ams_rows_visibility();
}

static void close_confirm_modal() {
    lv_obj_add_flag(g_confirm_modal, LV_OBJ_FLAG_HIDDEN);
}

static void confirm_cancel_cb(lv_event_t* e) {
    close_confirm_modal();
}

// Only queues the print request - see g_print_requested above for why the
// actual bambu_cmd_start_*() (MQTT publish) call must not happen here.
static void confirm_print_cb(lv_event_t* e) {
    g_pending_ftp_path = g_confirm_ftp_path;
    g_pending_is_gcode = !g_confirm_is_3mf;
    g_pending_use_ams = false;

    if (g_confirm_is_3mf && g_ams_enabled) {
        // Collect the non-"Skip" picks in row order, then right-align them
        // into the 5-element ams_mapping array per Bambu's scheme (see
        // bambu_mqtt.h) - the last color picked ends up in the last slot,
        // regardless of how many rows were left on "Skip".
        int chosen[4];
        int k = 0;
        for (int i = 0; i < 4; i++) {
            int val = g_color_options[g_color_option_idx[i]];
            if (val != -1) chosen[k++] = val;
        }
        if (k > 0) {
            int mapping[5] = {-1, -1, -1, -1, -1};
            for (int i = 0; i < k; i++) mapping[5 - k + i] = chosen[i];
            memcpy(g_pending_mapping, mapping, sizeof(mapping));
            g_pending_use_ams = true;
        }
    }

    g_print_requested = true;
    close_confirm_modal();
    // Jump back to the status screen - just a screen swap, no blocking work,
    // so this part is fine to do directly (same as every other back button).
    lv_scr_load(g_main_screen);
}

static void open_confirm_modal(const FtpEntry& entry) {
    g_confirm_ftp_path = join_path(g_current_path, entry.name);
    g_confirm_is_3mf = has_ext(entry.name, ".3mf");

    lv_label_set_text_fmt(g_confirm_file_label, "%s  (%s)", entry.name.c_str(), format_size(entry.size).c_str());

    bool is_gcode = has_ext(entry.name, ".gcode");
    if (g_confirm_is_3mf) {
        lv_obj_clear_flag(g_ams_toggle_btn, LV_OBJ_FLAG_HIDDEN);
        build_color_options();
        g_ams_enabled = false;
        for (int i = 0; i < 4; i++) {
            // Default: color N -> the Nth available tray in order, so a
            // straightforward "reprint with the AMS I already have loaded"
            // usually needs no changes beyond turning "Use AMS" on.
            g_color_option_idx[i] = (i + 1 < g_color_option_count) ? (i + 1) : 0;
            update_color_row_label(i);
        }
        update_ams_rows_visibility();
    } else {
        lv_obj_add_flag(g_ams_toggle_btn, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < 4; i++) lv_obj_add_flag(g_color_rows[i], LV_OBJ_FLAG_HIDDEN);
        (void)is_gcode;
    }

    lv_obj_clear_flag(g_confirm_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_confirm_modal);
}

static void render_page() {
    int total_pages = (g_entry_count + PAGE_SIZE - 1) / PAGE_SIZE;
    if (total_pages < 1) total_pages = 1;
    if (g_page >= total_pages) g_page = total_pages - 1;
    if (g_page < 0) g_page = 0;

    for (int i = 0; i < PAGE_SIZE; i++) {
        int idx = g_page * PAGE_SIZE + i;
        if (idx >= g_entry_count) {
            lv_obj_add_flag(g_row_btns[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(g_row_btns[i], LV_OBJ_FLAG_HIDDEN);
        FtpEntry& entry = g_entries[idx];
        if (entry.is_dir) {
            lv_label_set_text_fmt(g_row_labels[i], "[DIR]  %s", entry.name.c_str());
        } else {
            lv_label_set_text_fmt(g_row_labels[i], "%s   (%s)", entry.name.c_str(), format_size(entry.size).c_str());
        }
        lv_obj_set_user_data(g_row_btns[i], (void*)(uintptr_t)idx);
    }

    lv_label_set_text_fmt(g_page_label, T("files.page"), g_page + 1, total_pages, g_entry_count);
    lv_label_set_text(g_path_label, g_current_path.c_str());
    if (g_current_path == "/") lv_obj_add_state(g_up_btn, LV_STATE_DISABLED);
    else lv_obj_clear_state(g_up_btn, LV_STATE_DISABLED);
}

// Only queues the request - see g_list_requested above for why the actual
// bambu_ftp_list() call must not happen here.
static void request_list_refresh() {
    lv_label_set_text(g_status_label, T("loading"));
    g_list_requested = true;
}

static void row_click_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int idx = (int)(uintptr_t)lv_obj_get_user_data(btn);
    if (idx < 0 || idx >= g_entry_count) return;
    FtpEntry& entry = g_entries[idx];
    if (entry.is_dir) {
        g_current_path = join_path(g_current_path, entry.name);
        request_list_refresh();
    } else if (g_lan_mode) {
        // LAN-only mode: the firmware lets a third-party tool start a print, so open
        // the confirm dialog (AMS mapping and all - it was built and just wasn't
        // reachable). In cloud mode this same call is silently ignored by 1.08+,
        // which is why it stays behind the toggle.
        open_confirm_modal(entry);
    } else {
        // Cloud/default: 1.08+ blocks third-party print start, so this is browse-only.
        lv_label_set_text(g_status_label, T("files.print_blocked"));
    }
}

static void up_btn_cb(lv_event_t* e) {
    g_current_path = parent_path(g_current_path);
    request_list_refresh();
}

static void refresh_btn_cb(lv_event_t* e) {
    request_list_refresh();
}

static void page_prev_cb(lv_event_t* e) {
    g_page--;
    render_page();
}

static void page_next_cb(lv_event_t* e) {
    g_page++;
    render_page();
}

void create_files_ui() {
    if (g_files_screen) {
        lv_obj_del(g_files_screen);
        g_files_screen = nullptr;
    }

    g_files_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_files_screen, lv_color_hex(0x101418), LV_PART_MAIN);

    lv_obj_t* root = lv_obj_create(g_files_screen);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_center(root);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(root, PT_SZ(6), LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    // Not scrollable - paging buttons are used instead, same reasoning as
    // the Settings/Filament screens (a scrollable column tears on this
    // display's partial-refresh render mode).
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // --- Header ---
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_set_size(header, lv_pct(100), PT_SZ(36));
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, T("nav.files"));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t* back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, PT_SZ(80), PT_SZ(30));
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, T("back"));
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    // --- Path row: current folder + Up + Refresh ---
    lv_obj_t* path_row = lv_obj_create(root);
    lv_obj_set_size(path_row, lv_pct(100), PT_SZ(34));
    lv_obj_set_style_bg_opa(path_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(path_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(path_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(path_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(path_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(path_row, LV_OBJ_FLAG_SCROLLABLE);

    g_path_label = lv_label_create(path_row);
    lv_label_set_text(g_path_label, "/");
    lv_obj_set_style_text_font(g_path_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_path_label, lv_color_hex(0xAAAAAA), 0);

    lv_obj_t* btn_group = lv_obj_create(path_row);
    lv_obj_set_size(btn_group, PT_SZ(180), PT_SZ(30));
    lv_obj_set_style_bg_opa(btn_group, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_group, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn_group, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(btn_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(btn_group, PT_SZ(8), LV_PART_MAIN);
    lv_obj_clear_flag(btn_group, LV_OBJ_FLAG_SCROLLABLE);

    g_up_btn = lv_btn_create(btn_group);
    lv_obj_set_size(g_up_btn, PT_SZ(80), PT_SZ(30));
    lv_obj_set_style_bg_color(g_up_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* up_label = lv_label_create(g_up_btn);
    lv_label_set_text(up_label, "Up");
    lv_obj_set_style_text_font(up_label, &lv_font_montserrat_14, 0);
    lv_obj_center(up_label);
    lv_obj_add_event_cb(g_up_btn, up_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* refresh_btn = lv_btn_create(btn_group);
    lv_obj_set_size(refresh_btn, PT_SZ(90), PT_SZ(30));
    lv_obj_set_style_bg_color(refresh_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_label, T("refresh"));
    lv_obj_set_style_text_font(refresh_label, &lv_font_montserrat_14, 0);
    lv_obj_center(refresh_label);
    lv_obj_add_event_cb(refresh_btn, refresh_btn_cb, LV_EVENT_CLICKED, NULL);

    // --- File/folder rows (fixed set of PAGE_SIZE buttons, shown/hidden) ---
    for (int i = 0; i < PAGE_SIZE; i++) {
        lv_obj_t* row = lv_btn_create(root);
        lv_obj_set_size(row, lv_pct(100), PT_SZ(34));
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1c1c1c), LV_PART_MAIN);
        lv_obj_t* label = lv_label_create(row);
        lv_label_set_text(label, "");
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED, NULL);
        g_row_btns[i] = row;
        g_row_labels[i] = label;
    }

    // --- Paging + status row ---
    lv_obj_t* page_row = lv_obj_create(root);
    lv_obj_set_size(page_row, lv_pct(100), PT_SZ(30));
    lv_obj_set_style_bg_opa(page_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(page_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(page_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(page_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(page_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(page_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* prev_btn = lv_btn_create(page_row);
    lv_obj_set_size(prev_btn, PT_SZ(70), PT_SZ(28));
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, T("files.prev"));
    lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_12, 0);
    lv_obj_center(prev_label);
    lv_obj_add_event_cb(prev_btn, page_prev_cb, LV_EVENT_CLICKED, NULL);

    g_page_label = lv_label_create(page_row);
    lv_label_set_text(g_page_label, "");
    lv_obj_set_style_text_font(g_page_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_page_label, lv_color_hex(0x999999), 0);

    lv_obj_t* next_btn = lv_btn_create(page_row);
    lv_obj_set_size(next_btn, PT_SZ(70), PT_SZ(28));
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_t* next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, T("files.next"));
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_12, 0);
    lv_obj_center(next_label);
    lv_obj_add_event_cb(next_btn, page_next_cb, LV_EVENT_CLICKED, NULL);

    g_status_label = lv_label_create(root);
    lv_label_set_text(g_status_label, "");
    lv_obj_set_style_text_font(g_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xe74c3c), 0);
    lv_label_set_long_mode(g_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_status_label, lv_pct(100));

    // --- Print-confirm modal ---
    g_confirm_modal = lv_obj_create(g_files_screen);
    lv_obj_set_size(g_confirm_modal, PT_SZ(460), PT_SZ(320));
    lv_obj_center(g_confirm_modal);
    lv_obj_set_style_bg_color(g_confirm_modal, lv_color_hex(0x22262c), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_confirm_modal, lv_color_hex(0x3465a4), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_confirm_modal, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_confirm_modal, PT_SZ(14), LV_PART_MAIN);
    lv_obj_set_style_pad_gap(g_confirm_modal, PT_SZ(8), LV_PART_MAIN);
    lv_obj_set_flex_flow(g_confirm_modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(g_confirm_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_confirm_modal, LV_OBJ_FLAG_HIDDEN);

    g_confirm_file_label = lv_label_create(g_confirm_modal);
    lv_label_set_text(g_confirm_file_label, "");
    lv_obj_set_style_text_font(g_confirm_file_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_confirm_file_label, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_long_mode(g_confirm_file_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_confirm_file_label, lv_pct(100));

    g_ams_toggle_btn = lv_btn_create(g_confirm_modal);
    lv_obj_set_size(g_ams_toggle_btn, lv_pct(100), PT_SZ(34));
    lv_obj_set_style_bg_color(g_ams_toggle_btn, lv_color_hex(0x333333), LV_PART_MAIN);
    g_ams_toggle_label = lv_label_create(g_ams_toggle_btn);
    lv_label_set_text(g_ams_toggle_label, T("files.ams_off"));
    lv_obj_set_style_text_font(g_ams_toggle_label, &lv_font_montserrat_14, 0);
    lv_obj_center(g_ams_toggle_label);
    lv_obj_add_event_cb(g_ams_toggle_btn, ams_toggle_cb, LV_EVENT_CLICKED, NULL);

    for (int i = 0; i < 4; i++) {
        lv_obj_t* row = lv_obj_create(g_confirm_modal);
        lv_obj_set_size(row, lv_pct(100), PT_SZ(30));
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text_fmt(lbl, T("files.color"), i + 1);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xAAAAAA), 0);

        lv_obj_t* cyc_btn = lv_btn_create(row);
        lv_obj_set_size(cyc_btn, PT_SZ(160), PT_SZ(28));
        lv_obj_set_style_bg_color(cyc_btn, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_t* cyc_label = lv_label_create(cyc_btn);
        lv_label_set_text(cyc_label, T("files.skip"));
        lv_obj_set_style_text_font(cyc_label, &lv_font_montserrat_14, 0);
        lv_obj_center(cyc_label);
        lv_obj_add_event_cb(cyc_btn, color_cycle_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        g_color_rows[i] = row;
        g_color_value_labels[i] = cyc_label;
    }

    lv_obj_t* confirm_btn_row = lv_obj_create(g_confirm_modal);
    lv_obj_set_size(confirm_btn_row, lv_pct(100), PT_SZ(44));
    lv_obj_set_style_bg_opa(confirm_btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(confirm_btn_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(confirm_btn_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(confirm_btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(confirm_btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(confirm_btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* cancel_btn = lv_btn_create(confirm_btn_row);
    lv_obj_set_size(cancel_btn, PT_SZ(120), PT_SZ(40));
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, T("cancel"));
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(cancel_btn, confirm_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* print_btn = lv_btn_create(confirm_btn_row);
    lv_obj_set_size(print_btn, PT_SZ(160), PT_SZ(40));
    lv_obj_set_style_bg_color(print_btn, lv_color_hex(0x2ecc71), LV_PART_MAIN);
    lv_obj_t* print_label = lv_label_create(print_btn);
    lv_label_set_text(print_label, T("files.print"));
    lv_obj_set_style_text_font(print_label, &lv_font_montserrat_14, 0);
    lv_obj_center(print_label);
    lv_obj_add_event_cb(print_btn, confirm_print_cb, LV_EVENT_CLICKED, NULL);

    lv_scr_load(g_files_screen);
    g_current_path = "/";
    request_list_refresh();
}

void ui_files_loop() {
    if (!g_files_screen) return;

    if (g_list_requested) {
        g_list_requested = false;
        String err;
        bool ok = bambu_ftp_list(g_current_path.c_str(), g_entries, FTP_MAX_ENTRIES, &g_entry_count, &err);
        g_page = 0;
        if (!ok) {
            g_entry_count = 0;
            lv_label_set_text(g_status_label, err.c_str());
        } else {
            lv_label_set_text(g_status_label, "");
        }
        render_page();
    }

    if (g_print_requested) {
        g_print_requested = false;
        if (g_pending_is_gcode) {
            bambu_cmd_start_gcode_file(g_pending_ftp_path);
        } else if (g_pending_use_ams) {
            bambu_cmd_start_project_file(g_pending_ftp_path, true, g_pending_mapping);
        } else {
            bambu_cmd_start_project_file(g_pending_ftp_path, false, nullptr);
        }
    }
}
